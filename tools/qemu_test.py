# Drive a headless LeoOS desktop in QEMU through its monitor: type, click, drag and take screenshots.
#   qemu-system-i386 -cdrom leo-os.iso -m 128M -display none -monitor unix:/tmp/leo-mon.sock,server,nowait &
#   python3 tools/qemu_test.py /tmp/leo-mon.sock 'click:600,200' 'type:2+3\n' 'shot:20-focus' ...
# Commands: click:x,y  rclick:x,y  drag:x1,y1,x2,y2  move:x,y  type:text  key:qemu-key  sleep:s  shot:name
import socket, sys, time
from PIL import Image
KEYMAP = {' ':'spc','-':'minus','=':'equal','.':'dot',',':'comma','/':'slash',';':'semicolon','\n':'ret',
          '*':'shift-8','(':'shift-9',')':'shift-0','+':'shift-equal','%':'shift-5','_':'shift-minus','!':'shift-1',
          '\'':'apostrophe',':':'shift-semicolon','"':'shift-2','<':'shift-comma','>':'shift-dot','?':'shift-slash'}
s = socket.socket(socket.AF_UNIX); s.connect(sys.argv[1]); time.sleep(0.3); s.recv(65536)
pos = None
def cmd(c, wait=0.02):
    s.sendall((c + '\n').encode()); time.sleep(wait)
    try:
        s.setblocking(False); s.recv(65536)
    except BlockingIOError: pass
    finally: s.setblocking(True)
def rel(dx, dy):
    while dx or dy:
        sx = max(-100, min(100, dx)); sy = max(-100, min(100, dy))
        cmd('mouse_move %d %d' % (sx, sy), 0.01); dx -= sx; dy -= sy
def move(x, y):
    global pos
    if pos is None: rel(-3000, -3000); pos = (0, 0)
    rel(x - pos[0], y - pos[1]); pos = (x, y); time.sleep(0.05)
def key(ch):
    k = KEYMAP.get(ch) or ('shift-' + ch.lower() if ch.isupper() else ch)
    cmd('sendkey ' + k, 0.04)
for a in sys.argv[2:]:
    op, _, arg = a.partition(':')
    if op == 'click' or op == 'rclick':
        x, y = map(int, arg.split(',')); move(x, y)
        b = 1 if op == 'click' else 2
        cmd('mouse_button %d' % b, 0.08); cmd('mouse_button 0', 0.15)
    elif op == 'drag':
        x1, y1, x2, y2 = map(int, arg.split(',')); move(x1, y1); cmd('mouse_button 1', 0.08)
        steps = 12
        for i in range(1, steps + 1):
            nx = x1 + (x2 - x1) * i // steps; ny = y1 + (y2 - y1) * i // steps
            rel(nx - pos[0], ny - pos[1]); pos = (nx, ny); time.sleep(0.02)
        cmd('mouse_button 0', 0.15)
    elif op == 'move':
        x, y = map(int, arg.split(',')); move(x, y)
    elif op == 'type':
        for ch in arg.replace('\\n', '\n'): key(ch)
        time.sleep(0.3)
    elif op == 'key': cmd('sendkey ' + arg, 0.08)
    elif op == 'sleep': time.sleep(float(arg))
    elif op == 'shot':
        cmd('screendump /tmp/lt/%s.ppm' % arg, 0.8)
        Image.open('/tmp/lt/%s.ppm' % arg).save('screenshots/%s.png' % arg)
