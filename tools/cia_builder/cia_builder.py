#!/usr/bin/env python3
"""OpenCMR2 CIA Builder: puts your Colin McRae Rally 2.0 PC files on a 3DS.

Point it at the game's CMR2.exe. It checks the installation, then either
  * builds a standalone CIA with the game files inside (romfs), or
  * copies the game files to an SD card (sd:/3ds/OpenCMR2/Data/).

The game reads romfs:/ first and the SD card second (src/platform/sys.cpp).
Nothing here leaves your PC: the CIA holds your copy of the game, so keep it
to yourself.

Command line (the window is cia_builder_gui.py):
  python3 cia_builder.py --exe "<install>/CMR2.exe" --cia OpenCMR2-full.cia
  python3 cia_builder.py --exe "<install>/CMR2.exe" --sd E:/
  python3 cia_builder.py --exe "<install>/CMR2.exe" --check

Python 3.8 or later, no other packages. The CIA needs the kit next to this
file (kit/: opencmr2.elf, opencmr2.rsf, icon.icn, banner.bnr, bin/<os>/makerom),
as in the OpenCMR2-CIA-Builder download.
"""

import argparse
import hashlib
import os
import platform
import shutil
import stat
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, List, Optional

# SHA-256 of executables known to go with game files that work.
KNOWN_EXECUTABLES = {
    "e6e45465882495b20d5664c309ed5216cc6e558bb5bc0187e928176395e857d0": "this fork's reference copy",
}

# The files the game checks for before it starts (MissingGameData, src/platform/sys.cpp).
REQUIRED_FILES = ["Game/Bigfiles/common.bfl", "FrontEnd/common.bfl", "Sounds/surface.bfl"]

# Folders not used on the 3DS (the videos are skipped there).
SKIPPED_FOLDERS = {"movies"}
# Files at the top of the installation that are the PC program, not game data.
SKIPPED_SUFFIXES = {".exe", ".dll", ".ico", ".lnk", ".url", ".pdf", ".htm", ".html", ".msi", ".isu"}

SD_DATA_DIR = Path("3ds") / "OpenCMR2" / "Data"

Progress = Callable[[str, float], None]   # message, fraction done (0..1, or -1: unknown)


@dataclass
class Install:
    exe: Path
    root: Path
    sha256: str
    known: Optional[str]
    missing: List[str]
    files: List[Path] = field(default_factory=list)   # relative to root
    size: int = 0

    @property
    def ok(self) -> bool:
        return not self.missing and bool(self.files)


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as file:
        for block in iter(lambda: file.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def find_case_insensitive(root: Path, relative: str) -> Optional[Path]:
    """The game's own lookup ignores case (FindUnder); so does this one."""
    current = root
    for part in relative.split("/"):
        if not current.is_dir():
            return None
        match = next((child for child in current.iterdir() if child.name.lower() == part.lower()), None)
        if match is None:
            return None
        current = match
    return current


def game_files(root: Path) -> List[Path]:
    """Every file the 3DS needs, relative to the installation."""
    files = []
    for entry in sorted(root.iterdir(), key=lambda p: p.name.lower()):
        if entry.is_dir():
            if entry.name.lower() in SKIPPED_FOLDERS:
                continue
            for dirpath, dirnames, filenames in os.walk(entry):
                dirnames.sort(key=str.lower)
                for name in sorted(filenames, key=str.lower):
                    files.append(Path(dirpath, name).relative_to(root))
        elif entry.is_file():
            if entry.suffix.lower() in SKIPPED_SUFFIXES or entry.name.lower().startswith("unins"):
                continue
            files.append(entry.relative_to(root))
    return files


def inspect(exe: Path) -> Install:
    exe = Path(exe).expanduser().resolve()
    if not exe.is_file():
        raise FileNotFoundError(f"no file {exe}")
    root = exe.parent
    digest = sha256_of(exe)
    missing = []
    for relative in REQUIRED_FILES:
        found = find_case_insensitive(root, relative)
        if found is None or not found.is_file() or found.stat().st_size == 0:
            missing.append(relative)
    install = Install(exe=exe, root=root, sha256=digest, known=KNOWN_EXECUTABLES.get(digest), missing=missing)
    if not missing:
        install.files = game_files(root)
        install.size = sum((root / f).stat().st_size for f in install.files)
    return install


def describe(install: Install) -> str:
    lines = [f"Executable: {install.exe}", f"SHA-256:    {install.sha256}"]
    if install.known:
        lines.append(f"Known:      yes ({install.known})")
    else:
        lines.append("Known:      no -- it may still work; the game checks its own files")
    if install.missing:
        lines.append("Missing:    " + ", ".join(install.missing))
        lines.append("            (is the game installed, not just the CD's installer files?)")
    else:
        lines.append(f"Game files: {len(install.files)} files, {install.size / 2**20:.1f} MB (Movies left out)")
    return "\n".join(lines)


# ---- SD card --------------------------------------------------------------

def build_sd(install: Install, sd_root: Path, progress: Progress) -> Path:
    """Copies the game files to <sd_root>/3ds/OpenCMR2/Data/; files already
    there with the same size and date are kept, so running it again is quick."""
    target = Path(sd_root) / SD_DATA_DIR
    free = shutil.disk_usage(Path(sd_root)).free
    needed = sum((install.root / f).stat().st_size for f in install.files if not _same(install.root / f, target / f))
    if needed > free:
        raise OSError(f"not enough space on {sd_root}: {needed / 2**20:.0f} MB needed, {free / 2**20:.0f} MB free")
    done = 0
    for index, relative in enumerate(install.files):
        source, destination = install.root / relative, target / relative
        if not _same(source, destination):
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
        done += source.stat().st_size
        if index % 20 == 0 or index == len(install.files) - 1:
            progress(f"Copying {relative.as_posix()}", done / max(install.size, 1))
    progress(f"Game files in {target}", 1.0)
    return target


def _same(source: Path, destination: Path) -> bool:
    try:
        a, b = source.stat(), destination.stat()
    except OSError:
        return False
    # FAT keeps times to 2 s.
    return a.st_size == b.st_size and abs(a.st_mtime - b.st_mtime) <= 2


# ---- standalone CIA -------------------------------------------------------

def default_kit() -> Path:
    return Path(__file__).resolve().parent / "kit"


def makerom_path(kit: Path) -> Path:
    system = platform.system()
    if system == "Windows":
        return kit / "bin" / "windows-x86_64" / "makerom.exe"
    if system == "Linux":
        return kit / "bin" / "linux-x86_64" / "makerom"
    raise OSError(f"no makerom for {system} in the kit; put one on the PATH")


def check_kit(kit: Path) -> List[str]:
    missing = [name for name in ("opencmr2.elf", "opencmr2.rsf", "icon.icn", "banner.bnr") if not (kit / name).is_file()]
    try:
        tool = makerom_path(kit)
        if not tool.is_file() and shutil.which("makerom") is None:
            missing.append(str(tool.relative_to(kit)))
    except OSError as error:
        if shutil.which("makerom") is None:
            missing.append(str(error))
    return missing


def build_cia(install: Install, kit: Path, output: Path, progress: Progress, work: Optional[Path] = None) -> Path:
    kit = Path(kit)
    missing = check_kit(kit)
    if missing:
        raise FileNotFoundError(f"the kit in {kit} lacks: " + ", ".join(missing))
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    free = shutil.disk_usage(output.parent).free
    # The staged files and the CIA: twice the data, at most.
    if install.size * 2 > free:
        raise OSError(f"not enough space for the CIA: about {install.size * 2 / 2**20:.0f} MB needed, "
                      f"{free / 2**20:.0f} MB free in {output.parent}")

    with tempfile.TemporaryDirectory(prefix="opencmr2-cia-", dir=work) as temp:
        temp = Path(temp)
        romfs = temp / "romfs"
        _stage(install, romfs, progress)

        # The game's settings with the files as the CIA's romfs.
        rsf = temp / "opencmr2.rsf"
        text = (kit / "opencmr2.rsf").read_text(encoding="utf-8")
        if "\nRomFs:" in "\n" + text:
            raise ValueError("the kit's opencmr2.rsf already has a RomFs section")
        rsf.write_text(text.rstrip("\n") + f'\n\nRomFs:\n  RootPath: "{romfs.as_posix()}"\n', encoding="utf-8")

        tool = makerom_path(kit)
        if not tool.is_file():
            tool = Path(shutil.which("makerom"))
        if os.name != "nt":
            tool.chmod(tool.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
        command = [str(tool), "-f", "cia", "-o", str(output), "-target", "t", "-exefslogo",
                   "-elf", str(kit / "opencmr2.elf"), "-rsf", str(rsf),
                   "-icon", str(kit / "icon.icn"), "-banner", str(kit / "banner.bnr")]
        progress(f"Making the CIA with makerom ({install.size / 2**20:.1f} MB of game files)...", -1)
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if result.returncode != 0 or not output.is_file():
            raise RuntimeError("makerom failed:\n" + result.stdout.strip())
    progress(f"{output} ({output.stat().st_size / 2**20:.1f} MB): install it with FBI", 1.0)
    return output


def _stage(install: Install, romfs: Path, progress: Progress) -> None:
    """The romfs folder: hard links where the file system allows, copies otherwise."""
    done = 0
    for index, relative in enumerate(install.files):
        source, destination = install.root / relative, romfs / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        try:
            os.link(source, destination)
        except OSError:
            shutil.copyfile(source, destination)
        done += source.stat().st_size
        if index % 50 == 0 or index == len(install.files) - 1:
            progress(f"Preparing {relative.as_posix()}", done / max(install.size, 1))


# ---- command line ---------------------------------------------------------

def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--exe", required=True, type=Path, help="CMR2.exe of the PC installation")
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--cia", type=Path, help="build a standalone CIA at this path")
    action.add_argument("--sd", type=Path, help="copy the game files to this SD card (its root, e.g. E:/)")
    action.add_argument("--check", action="store_true", help="only check the installation")
    parser.add_argument("--kit", type=Path, default=default_kit(), help="the CIA kit (default: kit/ next to this file)")
    parser.add_argument("--work", type=Path, help="folder for temporary files (default: the system's)")
    args = parser.parse_args(argv)

    def progress(message: str, fraction: float) -> None:
        percent = f"{fraction * 100:5.1f}% " if fraction >= 0 else "       "
        print(f"{percent}{message}", flush=True)

    try:
        install = inspect(args.exe)
        print(describe(install))
        if not install.ok:
            return 1
        if args.check:
            return 0
        if args.sd:
            build_sd(install, args.sd, progress)
        else:
            build_cia(install, args.kit, args.cia, progress, args.work)
        return 0
    except (OSError, RuntimeError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
