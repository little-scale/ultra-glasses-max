#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if [ "${CONFIRM_VITURE_BINARY_TERMS:-}" != "yes" ]; then
    echo "Binary packaging is gated by the VITURE SDK distribution terms." >&2
    echo "Read docs/BINARY_DISTRIBUTION.md, then set CONFIRM_VITURE_BINARY_TERMS=yes." >&2
    exit 2
fi

if [ -z "${BINARY_EULA_FILE:-}" ] || [ ! -f "$BINARY_EULA_FILE" ]; then
    echo "Set BINARY_EULA_FILE to a reviewed end-user agreement." >&2
    exit 2
fi

"$project_root/scripts/build.sh"

version=$(/usr/bin/plutil -extract version raw "$project_root/package-info.json")
release_dir="$project_root/release"
archive="$release_dir/UltraGlassesMax-arm64-$version.zip"
checksum="$archive.sha256"
release_temp=$(mktemp -d "${TMPDIR:-/tmp}/ultra-glasses-max.XXXXXX")
trap '/bin/rm -rf "$release_temp"' EXIT HUP INT TERM

mkdir -p "$release_dir"
/usr/bin/ditto "$project_root/build/release/package" \
    "$release_temp/UltraGlassesMax"
/usr/bin/ditto "$BINARY_EULA_FILE" \
    "$release_temp/UltraGlassesMax/EULA.txt"
/bin/rm -f "$archive" "$checksum"
/usr/bin/ditto -c -k --sequesterRsrc --keepParent \
    "$release_temp/UltraGlassesMax" "$archive"
/usr/bin/shasum -a 256 "$archive" > "$checksum"

echo "Release candidate: $archive"
echo "Checksum: $checksum"
