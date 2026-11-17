#!/usr/bin/env bash
# Moves files between this PC and a 3DS running ftpd (FTP server, port 5000),
# for the logs and settings in sd:/3ds/OpenCMR2/. Needs only bash and curl:
# CachyOS/Linux, or Git Bash on Windows (curl ships with Windows 10).
#
#   tools/3ds.sh pull [--clean]    opencmr2.log and frames-*.csv to captures/<date-time>/
#                                  (--clean: then delete the frames-*.csv on the SD card)
#   tools/3ds.sh ls                list sd:/3ds/OpenCMR2/
#   tools/3ds.sh put FILE [DIR]    upload FILE to DIR (default /3ds/OpenCMR2/)
#   tools/3ds.sh get PATH [DEST]   download one file from the SD card
#   tools/3ds.sh ping              check that ftpd answers
#
# The console's address: CTR_HOST (and CTR_PORT, default 5000), or a line
# CTR_HOST=192.168.x.y in tools/3ds.env (not committed).
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(dirname "$here")"
[ -f "$here/3ds.env" ] && . "$here/3ds.env"
host="${CTR_HOST:-}"
port="${CTR_PORT:-5000}"
remote_dir="/3ds/OpenCMR2"

die() { echo "3ds.sh: $*" >&2; exit 1; }
[ -n "$host" ] || die "set CTR_HOST (the IP ftpd shows on the 3DS), e.g. CTR_HOST=192.168.1.50 tools/3ds.sh pull, or put it in tools/3ds.env"
command -v curl >/dev/null || die "curl not found"

base="ftp://$host:$port"
# ftpd accepts any user; short timeouts so a sleeping console fails fast.
ftp() { curl --silent --show-error --fail --connect-timeout 5 --ftp-pasv "$@"; }

cmd="${1:-}"
shift || true
case "$cmd" in
ping)
    ftp --list-only "$base/" >/dev/null && echo "ftpd at $host:$port answers"
    ;;
ls)
    ftp "$base$remote_dir/"
    ;;
get)
    [ $# -ge 1 ] || die "get PATH [DEST]"
    src="$1"; dest="${2:-$(basename "$src")}"
    ftp -o "$dest" "$base$src"
    echo "$src -> $dest"
    ;;
put)
    [ $# -ge 1 ] || die "put FILE [DIR]"
    [ -f "$1" ] || die "no file $1"
    dir="${2:-$remote_dir}"
    ftp --ftp-create-dirs -T "$1" "$base${dir%/}/"
    echo "$1 -> $dir/"
    ;;
pull)
    clean=0
    [ "${1:-}" = "--clean" ] && clean=1
    out="$root/captures/$(date +%Y%m%d-%H%M%S)"
    mkdir -p "$out"
    names="$(ftp --list-only "$base$remote_dir/" | tr -d '\r')"
    got=0
    while IFS= read -r name; do
        case "$name" in
        opencmr2.log|opencmr2.ini|frames-*.csv)
            ftp -o "$out/$name" "$base$remote_dir/$name"
            echo "  $name ($(wc -c <"$out/$name") bytes)"
            got=$((got + 1))
            ;;
        esac
    done <<<"$names"
    [ "$got" -gt 0 ] || { rmdir "$out"; die "nothing to pull in $remote_dir"; }
    echo "$got files in ${out#$root/}"
    if [ "$clean" = 1 ]; then
        while IFS= read -r name; do
            case "$name" in
            frames-*.csv) ftp -Q "DELE $remote_dir/$name" "$base/" -o /dev/null && echo "  deleted $name on the SD card" ;;
            esac
        done <<<"$names"
    fi
    ;;
*)
    sed -n '2,16p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    [ -z "$cmd" ] || exit 1
    ;;
esac
