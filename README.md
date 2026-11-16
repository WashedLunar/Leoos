# LeoOS 0.2 in the browser

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

## Publish on GitHub Pages (when you're ready)

The site is already laid out for Pages: `index.html` is at the root and there's a `.nojekyll` file. Push this repo to GitHub, then go to
Settings → Pages → Deploy from branch → `main` / root.

## Files

| Path | What |
|---|---|
| `index.html` | the page: emulator setup, touch→PS/2 mouse, on-screen keyboard, buttons |
| `leo-os.iso` | LeoOS 0.2 (copied from `/workspace/leo-os/leo-os.iso`) |
| `v86/libv86.js`, `v86/v86.wasm` | v86 emulator (BSD-2-Clause, see `v86/LICENSE`) |
| `bios/seabios.bin`, `bios/vgabios.bin` | SeaBIOS and VGA BIOS (LGPL) from the v86 repo |
| `screenshots/` | screenshots of the site running in headless Chrome |

To update the OS, rebuild in `../leo-os` and copy the new `leo-os.iso` over the one here.
