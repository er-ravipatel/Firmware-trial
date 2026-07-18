#!/usr/bin/env bash
# Map fault addresses to functions in kernel8.elf.
set -u
ELF="$1"; shift
SYMS="$(aarch64-linux-gnu-nm -n "$ELF" | grep -E ' [tT] ')"
for a in "$@"; do
    target=$((16#$a))
    echo "addr 0x$a:"
    echo "$SYMS" | awk -v t="$target" '
        { addr = strtonum("0x" $1) }
        addr <= t { fn = $3; fa = $1 }
        addr > t  { print "    in " fn " (starts 0x" fa ")"; exit }
    '
done
