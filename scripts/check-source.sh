#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project_root"

forbidden=$(find . \
    \( -path './.git' -o -path './build' -o -path './dist' -o -path './release' \) -prune -o \
    -type f \( -name 'libglasses.dylib' -o -name 'libcarina_vio.dylib' \
    -o -name '*.mxo' -o -name 'viture_glasses_sdk.zip' \) -print)
if [ -n "$forbidden" ]; then
    echo "Proprietary or generated binary files found:" >&2
    echo "$forbidden" >&2
    exit 1
fi

python3 - <<'PY'
import json
import pathlib

root = pathlib.Path('.')
excluded = {'.git', 'build', 'dist', 'release'}
personal_prefix = '/' + 'Users' + '/'
for path in root.rglob('*'):
    if not path.is_file() or any(part in excluded for part in path.parts):
        continue
    try:
        content = path.read_text(encoding='utf-8')
    except UnicodeDecodeError:
        continue
    if personal_prefix in content:
        raise SystemExit(f'personal absolute path found in {path}')

json_files = [root / 'package-info.json', *root.glob('help/*.maxhelp'), *root.glob('tests/*.maxpat')]
for path in json_files:
    with path.open(encoding='utf-8') as stream:
        json.load(stream)

package_version = json.loads((root / 'package-info.json').read_text())['version']
version_files = [
    root / 'CMakeLists.txt',
    root / 'source/viture.ultra/CMakeLists.txt',
    root / 'source/jit.viture.stereo/CMakeLists.txt',
]
for path in version_files:
    if package_version not in path.read_text():
        raise SystemExit(f'version {package_version} missing from {path}')

print(f'Source checks passed for version {package_version}.')
PY
