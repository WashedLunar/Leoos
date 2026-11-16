; LeoOS - boot.asm
; Multiboot header, kernel entry point and low-level CPU helpers.

MB_MAGIC    equ 0x1BADB002
%ifdef TEXT_MODE
MB_FLAGS    equ (1 << 0) | (1 << 1)         ; text-mode kernel: no video request
%else
MB_FLAGS    equ (1 << 0) | (1 << 1) | (1 << 2) ; page-align modules, memory info, video mode
%endif
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM
%ifndef TEXT_MODE
    dd 0, 0, 0, 0, 0                        ; address fields (unused: we are a normal ELF)
    dd 0                                    ; mode_type 0 = linear graphics framebuffer
    dd 1024                                 ; preferred width
    dd 768                                  ; preferred height
    dd 32                                   ; preferred bits per pixel
%endif

section .bss
align 16
stack_bottom:
    resb 16384                              ; 16 KiB kernel stack
stack_top:

section .text
global _start
extern kmain
_start:
    cli
    mov esp, stack_top
    push ebx                                ; multiboot info pointer
    push eax                                ; multiboot magic
    call kmain
.hang:
    cli
    hlt
    jmp .hang

; void gdt_flush(uint32_t gdt_ptr)
global gdt_flush
gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]
    mov ax, 0x10                            ; kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.reload_cs                     ; far jump to load kernel code selector
.reload_cs:
    ret

; void idt_load(uint32_t idt_ptr)
global idt_load
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; ---- Interrupt stubs -------------------------------------------------------
%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp common_stub
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1
    jmp common_stub
%endmacro

%macro IRQ 2
global irq%1
irq%1:
    push dword 0
    push dword %2
    jmp common_stub
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

extern interrupt_dispatch
common_stub:
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld
    push esp                                ; struct regs *
    call interrupt_dispatch
    add esp, 4
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8                              ; drop int number + error code
    iret

section .note.GNU-stack noalloc noexec nowrite progbits
