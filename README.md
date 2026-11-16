# LeoOS 0.4 in the browser

This static site runs [LeoOS](../leo-os) in the browser using the [v86](https://github.com/copy/v86) x86 emulator,
on a phone or a computer. There's no server-side code. It boots the real `leo-os.iso` as a CD-ROM with 128 MB RAM, and GRUB sets the same
1024x768x32 VBE mode it uses in QEMU.

## Run it locally

```sh
cd leo-os-web
python3 -m http.server 8086
# open http://localhost:8086 (or http://<your-computer's-IP>:8086 on a phone on the same Wi-Fi)
```

You can't open `index.html` straight from disk with `file://`: the browser needs to fetch the `.wasm`, BIOS and ISO files over HTTP.

## Controls

- **Phone:** tap to click, drag to move windows or draw in Paint. Tap **Keyboard** to open the phone's keyboard and type into the
  focused window. **Fullscreen** uses the Fullscreen API where it exists, or a full-page layout on iPhone. **Restart** reboots the VM
- **Computer:** use the mouse and keyboard on the screen as usual
- **Snake:** tap the on-screen arrows next to the board, or tap the board on the side you want to turn to (arrow keys or WASD on a computer). The walls wrap round
- **Minesweeper:** tap to dig, turn on **Flag** to place flags by tapping (right-click on a computer), and tap the smiley for a new game
- **Windows:** the **_** button minimises a window and its taskbar button brings it back. Drag the green corner tab to resize
- **Files:** browse folders, double-tap a file to open it in Notepad, or use New folder, Rename, Delete and Copy
- **Browser:** built-in LeoOS pages and notes you save in Notepad. Real websites are not available yet
- **2048 / Deep Dive / Tetris:** on-screen pads for phones; arrow keys or WASD on a computer

## Live site

GitHub Pages serves this repo's `main` branch at https://washedlunar.github.io/Leoos/

## Files

| Path | What |
|---|---|
| `index.html` | the page: emulator setup, touch→PS/2 mouse, on-screen keyboard, buttons |
| `leo-os.iso` | LeoOS 0.4 (built from `source/`) |
| `source/` | the LeoOS 0.4 source code: C kernel, NASM boot stub, Makefile and its README (build it with `make`) |
| `CHANGELOG.md` | what's new in 0.4, 0.3.1 and 0.3 |
| `v86/libv86.js`, `v86/v86.wasm` | v86 emulator (BSD-2-Clause, see `v86/LICENSE`) |
| `bios/seabios.bin`, `bios/vgabios.bin` | SeaBIOS and VGA BIOS (LGPL) from the v86 repo |
| `screenshots/` | screenshots of the site running in headless Chrome |

To update the OS, run `make` in `source/` and copy the new `leo-os.iso` over the one here.
