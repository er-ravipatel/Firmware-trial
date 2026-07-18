#!/usr/bin/env bash
# Resize all JPEGs in a directory to max 1920px (baseline), in place. For prepping photos that
# were dropped onto the card at full resolution.
set -u
DIR="${1:?usage: _resize_dir.sh <dir>}"
shopt -s nullglob nocaseglob
for f in "$DIR"/*.jpg "$DIR"/*.jpeg; do
    if convert "$f" -resize '1920x1920>' -quality 85 -interlace none "$f.tmp" 2>/dev/null; then
        mv "$f.tmp" "$f"
        echo "resized $(basename "$f")  ->  $(identify -format '%wx%h' "$f" 2>/dev/null)"
    else
        rm -f "$f.tmp"
        echo "SKIP $(basename "$f")"
    fi
done
shopt -u nullglob nocaseglob
