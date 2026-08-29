#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if [ -z "${VITURE_SDK_ROOT:-}" ]; then
    echo "VITURE_SDK_ROOT is not set." >&2
    echo "See docs/SDK_SETUP.md for setup instructions." >&2
    exit 2
fi

cmake --preset release -S "$project_root"
cmake --build --preset release
ctest --preset release

package_dir="$project_root/build/release/package"
/usr/bin/codesign --verify --deep --strict \
    "$package_dir/externals/viture.ultra.mxo"
/usr/bin/codesign --verify --deep --strict \
    "$package_dir/externals/jit.viture.stereo.mxo"

echo "Built and verified: $package_dir"
