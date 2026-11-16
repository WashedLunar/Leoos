/* LeoOS - PS/2 keyboard driver (IRQ1, scancode set 1, UK or US layout) */
#include "cpu.h"
#include "io.h"

/* 0x9C = '£' and 0xAA = '¬' in the VGA's built-in code page 437 font */
static const char uk_normal[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '#','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ', 0,
    [0x56] = '\\',
};
static const char uk_shift[128] = {
    0, 27, '!','"',(char)0x9C,'$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0, 'A','S','D','F','G','H','J','K','L',':','@',(char)0xAA,
    0, '~','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0, ' ', 0,
    [0x56] = '|',
};
static const char us_normal[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ', 0,
    [0x56] = '\\',
};
static const char us_shift[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0, 'A','S','D','F','G','H','J','K','L',':','"','~',
    0, '|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0, ' ', 0,
    [0x56] = '|',
};

#define BUF_SIZE 128
static volatile int buf[BUF_SIZE];
static volatile uint32_t head, tail;
static bool shift, caps, ctrl, extended;
static int uk = 1;

static void push(int c) {
    uint32_t next = (head + 1) % BUF_SIZE;
    if (next != tail) { buf[head] = c; head = next; }
}

static void keyboard_handler(struct regs *r) {
    (void)r;
    uint8_t sc = inb(0x60);
    if (sc == 0xE0) { extended = true; return; }
    bool release = sc & 0x80;
    uint8_t code = sc & 0x7F;

    if (extended) {
        extended = false;
        if (code == 0x1D) { ctrl = !release; return; }
        if (release) return;
        switch (code) {
        case 0x48: push(KEY_UP); break;
        case 0x50: push(KEY_DOWN); break;
        case 0x4B: push(KEY_LEFT); break;
        case 0x4D: push(KEY_RIGHT); break;
        case 0x1C: push('\n'); break;           /* keypad enter */
        case 0x35: push('/'); break;            /* keypad slash */
        }
        return;
    }
    switch (code) {
    case 0x2A: case 0x36: shift = !release; return;
    case 0x1D: ctrl = !release; return;
    case 0x3A: if (!release) caps = !caps; return;
    }
    if (release) return;

    const char *map = shift ? (uk ? uk_shift : us_shift) : (uk ? uk_normal : us_normal);
    char c = map[code];
    if (!c) return;
    if (caps) {
        if (c >= 'a' && c <= 'z') c -= 32;
        else if (c >= 'A' && c <= 'Z') c += 32;
    }
    if (ctrl && (c == 'l' || c == 'L')) { push(0x0C); return; }   /* Ctrl+L clears */
    if (ctrl && (c == 'c' || c == 'C')) { push(0x03); return; }   /* Ctrl+C cancels line */
    push((uint8_t)c);
}

int keyboard_getchar(void) {
    while (head == tail) cpu_hlt();             /* sleep until the next interrupt */
    int c = buf[tail];
    tail = (tail + 1) % BUF_SIZE;
    return c;
}

void keyboard_set_layout(int is_uk) { uk = is_uk; }
int  keyboard_is_uk(void) { return uk; }

void keyboard_init(void) {
    while (inb(0x64) & 1) inb(0x60);            /* flush pending bytes */
    irq_install_handler(1, keyboard_handler);
}
