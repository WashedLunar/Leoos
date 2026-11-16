# LeoOS 0.4 – Release notes

LeoOS 0.4 adds folders, a Files app, a pages browser and three new games, plus a much bigger Terminal. It still boots straight to the green desktop and runs in your phone's browser. Files are still kept only in RAM, so they disappear when you restart.

## Folders and Files (Nova)
- The RAM filesystem now has folders, a current directory and paths such as `docs/welcome.txt` or `/docs/welcome.txt`. There are 32 slots (files and folders) of up to 4000 bytes each.
- The new **Files** app works like a small File Explorer: a path bar and Up button, folder and file icons, double-click to open a folder or a file in Notepad, and toolbar buttons for New folder, Rename, Delete and Copy/Paste.
- Notepad can save into a folder (type a path like `docs/notes.txt`) and its Open dialog can browse folders.
- The Terminal's `ls`, `cat`, `rm` and `write` commands take paths. After you `mkdir` in the Terminal, Files refreshes its list when you look at it.

## Browser (Vega)
- A pages browser with built-in `leo://` pages (Home, What's new, Apps, Terminal tips, Files, and more), Back, Forward and Home, an address bar, and scrolling by dragging or the arrow keys.
- `file:` links open a page you wrote in Notepad, including files that live in a folder.
- Real `http://` addresses show a friendly "No internet yet" page: LeoOS still has no network driver.

## Games
- **2048** (Luna): merge the tiles on a 4×4 board. Swipe, use the arrow keys or WASD, or tap the on-screen pad. N or New starts again.
- **Deep Dive** (Luna): a little underwater game inspired by Subnautica. Swim with the mouse or the arrow keys, collect fish, dodge jellyfish and surface for air. Oxygen drains faster the deeper you go.
- **Tetris** (Starlink): a classic 10×20 well with a 7-bag randomiser, next-piece preview, ghost piece, score and level. Arrow keys or WASD, or a tap-friendly side pad. Space hard-drops; P pauses; N starts a new game.

## Terminal commands (Astra)
- New commands: `pwd`, `cd`, `mkdir`, `rmdir`, `tree`, `touch`, `cp`, `mv`, `wc`, `head`, `grep`, `date`, `time`, `whoami`, `hostname`, `uname`, `neofetch`, `open` (and shortcuts such as `snake`, `tetris`, `files`, `browser`), `fortune` and `cowsay`.
- `cp` and `mv` use the filesystem's copy and rename, so `cp a.txt docs` and `mv a.txt docs` work.
- The prompt shows the current folder: `leo@leoos:~$` at `/`, or `leo@leoos:/docs$` inside a folder.
- `date` reads the CMOS real-time clock. `help` is paged (`help` then `help 2`) so the longer list fits the 25-line Terminal.

## Thanks
Built together for Leo. Nova did the folders and the Files app, and put the release together. Luna wrote 2048 and Deep Dive (and Minesweeper in 0.3). Vega wrote the Browser. Astra wrote the new terminal commands. Starlink wrote Tetris.

# LeoOS 0.3.1 – Fixes

Starlink tested 0.3 on the live site and found three problems. They're all fixed:

- **Resizing windows now works reliably.** The corner grip was small (18 px, about 9 px on a laptop screen), and the web page's touch and mouse code let the LeoOS cursor drift a few pixels further from your finger with every move, because it kept fractions of a pixel the mouse can't move. Together that made it easy to miss the grip. The grip is now a bigger green tab you can grab from slightly inside or outside the corner, and the cursor no longer drifts.
- **Snake doesn't end straight away any more.** The snake started moving at 0.15 s per step and died at the first wall, so with the delay of tapping in the browser most games ended in about 2 seconds with a score of 0. The walls now wrap round, the snake starts in the middle, and it begins slower (0.22 s per step) and speeds up as you eat. Only biting your own tail ends the game.
- **Minesweeper's New game turns Flag mode off.**

# LeoOS 0.3 – Release notes

LeoOS 0.3 builds on the 0.2 desktop with better window handling, saving in Notepad and two new games. It still boots straight to the green desktop and runs in your phone's browser.

## Fixes
- **Typing goes to the window you clicked.** In 0.2, keyboard input could land in whichever window had focus before (for example, typing `2+3` for the Calculator ended up in the Terminal). Clicking anywhere in a window now focuses it and brings it to the front. Keyboard and mouse events are also handled in the exact order they arrive, so keys typed straight after a click always go to the new window.

## Windows
- **Minimise.** Every window has a minimise button. Minimised windows show as a darker taskbar button; tap it to bring the window back. Tapping the button of the window you're using minimises it.
- **Resize.** Drag the grip in the bottom-right corner of Terminal, Paint or Notepad to make the window bigger or smaller. Notepad re-wraps its text, Paint's canvas grows, and a small Terminal shows the part around the cursor.
- **Maximise.** Paint and Notepad also have a maximise button that fills the screen. Tap it again to restore the window.
- **Taskbar.** Buttons now show each app's icon and shrink to fit when lots of windows are open.

## Apps
- **Notepad: save and open.** Notepad has New, Open and Save buttons. Save asks for a file name (it suggests `notes.txt`), and Open shows a list of files to tap.
- **Files.** A small in-memory filesystem holds up to 16 files of up to 4000 bytes each, starting with `readme.txt`. The Terminal has new commands: `ls`, `cat <file>`, `rm <file>` and `write <file> <text>`. They work in the text-mode shell too. There's no disk yet, so files are lost when you restart LeoOS.
- **New game: Snake.** Steer with the big on-screen arrows (made for phones), by tapping the board on the side you want to turn to, or with the arrow keys or WASD. The snake gets faster as it eats. It shows your score and best score, has a Game over screen with New game, and pauses when you switch to another window.
- **New game: Minesweeper.** A 9×9 grid with 10 mines. Your first tap is always safe, and empty areas open up by themselves. The numbers are coloured and a counter shows how many mines are left. The Flag button lets you place flags with taps on a phone (or use right-click or the F key), the smiley starts a new game, and a banner tells you whether you won or lost.
- **About** shows version 0.3 and says that files are only kept until you restart.

## Thanks
Built by Nova for Leo. Minesweeper was written by Luna. Starlink wrote these release notes, and Luna and Starlink both tested 0.2 and found the focus bug.
