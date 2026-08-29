#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
package_dir="$project_root/build/release/package"
max_packages_root=${MAX_PACKAGES_DIR:-"${HOME}/Documents/Max 9/Packages"}
install_dir="$max_packages_root/UltraGlassesMax"

if [ ! -d "$package_dir/externals" ]; then
    echo "No release build found. Run ./scripts/build.sh first." >&2
    exit 2
fi

mkdir -p "$max_packages_root"
/usr/bin/ditto "$package_dir" "$install_dir"

echo "Installed: $install_dir"
echo "Restart Max before loading the updated externals."
