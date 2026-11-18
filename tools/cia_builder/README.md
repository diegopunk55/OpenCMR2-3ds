# OpenCMR2 CIA Builder

Puts your Colin McRae Rally 2.0 PC files on a New 3DS, in one of two ways:

- **Standalone CIA**: one `.cia` with the game files inside. Install it with
  FBI and play; nothing else goes on the SD card.
- **SD card folder**: the game files in `sd:/3ds/OpenCMR2/Data/`, for the
  plain `OpenCMR2.cia` from the build (a small file to reinstall when only
  the game's code changes).

The game looks in the CIA first and on the SD card second, so either works
with the same game version. Settings, saves and logs stay in
`sd:/3ds/OpenCMR2/` both ways.

The CIA holds your copy of the game: it is for your console only. Don't
share it.

## Use

1. Download **OpenCMR2-CIA-Builder** from a build of this repository
   (Actions → a green run → Artifacts) and extract it.
2. Start the window:
   - Windows: double-click `cia_builder_gui.py` (Python 3 from python.org).
   - Linux: `python3 cia_builder_gui.py` (needs Tk: `pacman -S tk` on
     CachyOS/Arch, `apt install python3-tk` on Debian/Ubuntu).
3. **Browse** to the `CMR2.exe` of your installed game. The window checks
   it (its SHA-256, and the files the game needs) and shows the size.
4. Pick the output (a `.cia` file, or the SD card's root) and press **Build**.

The `Movies` folder and the PC program files (`.exe`, `.dll`, ...) are left
out: the 3DS skips the videos.

Without the window:

```
python3 cia_builder.py --exe "<install>/CMR2.exe" --check
python3 cia_builder.py --exe "<install>/CMR2.exe" --cia OpenCMR2-full.cia
python3 cia_builder.py --exe "<install>/CMR2.exe" --sd E:/
```

Building the CIA needs about twice the game's size free (the files are
staged, then packed), and takes a while for a few hundred MB.

## What's inside the download

```
cia_builder.py, cia_builder_gui.py   the tool (Python 3.8+, no packages)
kit/opencmr2.elf                     the game, as built by CI
kit/opencmr2.rsf, icon.icn, banner.bnr   the CIA's settings, icon and banner
kit/bin/linux-x86_64/makerom         makerom (Project_CTR), Linux
kit/bin/windows-x86_64/makerom.exe   makerom, Windows
kit/VERSION                          the commit it was built from
```

The kit and the game must come from the same build; a newer game version
means a new download.

## Installations

The window says whether the `CMR2.exe` is known. Known executables
(`KNOWN_EXECUTABLES` in `cia_builder.py`):

| SHA-256 | |
| --- | --- |
| `e6e45465882495b20d5664c309ed5216cc6e558bb5bc0187e928176395e857d0` | this fork's reference copy |

An unknown one is not refused: the game checks its own files when it starts.
