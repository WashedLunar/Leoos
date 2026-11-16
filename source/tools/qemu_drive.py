# Drive a headless LeoOS in QEMU via its monitor socket: type text and take screenshots.
# Start QEMU first:  qemu-system-i386 -cdrom leo-os.iso -display none -monitor unix:mon.sock,server,nowait &
# Then e.g.:        python3 tools/qemu_drive.py 'help\n' 'uptime\n' shot:demo   (needs Pillow)
import socket, sys, time
from PIL import Image
KEYMAP = {' ':'spc','-':'minus','=':'equal','.':'dot',',':'comma','/':'slash',';':'semicolon','\n':'ret',
          '*':'shift-8','(':'shift-9',')':'shift-0','+':'shift-equal','%':'shift-5','_':'shift-minus','!':'shift-1','\'':'apostrophe',':':'shift-semicolon','"':'shift-2'}
s = socket.socket(socket.AF_UNIX); s.connect('mon.sock'); time.sleep(0.3); s.recv(65536)
def cmd(c):
    s.sendall((c+'\n').encode()); time.sleep(0.05)
def key(ch):
    if ch in KEYMAP: k = KEYMAP[ch]
    elif ch.isupper(): k = 'shift-' + ch.lower()
    else: k = ch
    cmd('sendkey ' + k); time.sleep(0.03)
def typ(text):
    for ch in text: key(ch)
    time.sleep(0.4)
def shot(name):
    cmd('screendump ./%s.ppm' % name); time.sleep(0.8)
    Image.open('./%s.ppm' % name).save('screenshots/%s.png' % name)
for a in sys.argv[1:]:
    if a.startswith('shot:'): shot(a[5:])
    elif a.startswith('sleep:'): time.sleep(float(a[6:]))
    elif a.startswith('raw:'): cmd(a[4:]); time.sleep(0.3)
    else: typ(a.replace('\\n', '\n'))
