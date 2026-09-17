#!/usr/bin/env bash
# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT
set -euo pipefail

repo=$(cd "$(dirname "$0")/../.." && pwd)
cd "$repo"
if [[ $(uname -s) != Darwin ]]; then
  echo 'This preparation script requires macOS.' >&2
  exit 1
fi
if [[ -n $(git status --porcelain) ]]; then
  echo 'Commit or isolate working-tree changes before preparing a revision-labelled artifact.' >&2
  exit 1
fi

revision=$(git rev-parse HEAD)
short_revision=$(git rev-parse --short=8 HEAD)
package_dir="$repo/dist/macos-$(uname -m)-$short_revision"
build_dir="$repo/build"
stage_dir="$package_dir/stage"
if [[ -e "$package_dir" ]]; then
  echo "Output already exists; preserve or move it before rebuilding: $package_dir" >&2
  exit 1
fi
deploy_qt=$(command -v macdeployqt)
qt_svg_lib="$(brew --prefix qtsvg)/lib"
mkdir -p "$stage_dir"

# Reconfigure on every package build so the embedded SHA matches the source.
cmake -S "$repo" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTS=ON -DSKIP_BUILD_TESTS=ON \
  -DBUILD_INSTALLER=OFF -DBUILD_OSX_BUNDLE=ON \
  -DCMAKE_INSTALL_PREFIX="$stage_dir" -DDEPLOYQT=/usr/bin/true
cmake --build "$build_dir" --parallel 4 --target \
  Deskflow deskflow-core app_translations OSXCursorControllerTests \
  EventQueueTests ServerTests ServerProxyTests OSXKeyStateTests KeyStateTests

QT_QPA_PLATFORM=offscreen ctest --test-dir "$build_dir/src/unittests" --output-on-failure \
  -R '^(OSXCursorControllerTests|EventQueueTests|ServerTests|ServerProxyTests|KeyStateTests)$'
QT_QPA_PLATFORM=offscreen "$build_dir/src/unittests/platform/OSXKeyStateTests" mapModifiersFromOSX_OSXMask
git diff --exit-code
git diff --check

# Installation here means staging inside dist, never the live Applications directory.
cmake --install "$build_dir" --prefix "$stage_dir"
bundle="$stage_dir/Deskflow.app"
core="$bundle/Contents/MacOS/deskflow-core"
# Deploy once, including the separate Homebrew QtSvg framework search path.
mkdir -p "$bundle/Contents/Frameworks"
ditto "$qt_svg_lib/QtSvg.framework" "$bundle/Contents/Frameworks/QtSvg.framework"
"$deploy_qt" "$bundle" "-executable=$core" \
  "-executable=$bundle/Contents/Frameworks/QtSvg.framework/Versions/A/QtSvg" \
  "-libpath=$qt_svg_lib" '-codesign=-'
codesign --verify --deep --strict "$bundle"

# Refuse a supposedly standalone bundle that still needs this build tree or Brew.
python3 - "$bundle" <<'PY'
import pathlib, subprocess, sys
root = pathlib.Path(sys.argv[1])
magic = {bytes.fromhex(x) for x in ['feedface', 'cefaedfe', 'feedfacf', 'cffaedfe', 'cafebabe', 'bebafeca', 'cafebabf', 'bfbafeca']}
count = 0
for p in root.rglob('*'):
    if not p.is_file() or p.is_symlink():
        continue
    with p.open('rb') as f:
        if f.read(4) not in magic:
            continue
    count += 1
    # LC_ID_DYLIB is the library's identity, not a loaded dependency.
    identities = subprocess.check_output(['otool', '-D', str(p)], text=True).splitlines()[1:]
    deps = subprocess.check_output(['otool', '-L', str(p)], text=True).splitlines()[1:]
    for line in deps:
        dep = line.strip().split(' (', 1)[0]
        if dep in identities:
            continue
        if dep.startswith('@loader_path/') and not (p.parent / dep.removeprefix('@loader_path/')).exists():
            raise SystemExit(f'Missing loader-relative dependency in {p.relative_to(root)}: {dep}')
        if dep.startswith('@rpath/'):
            suffix = dep.removeprefix('@rpath/')
            if not any((root / 'Contents' / folder / suffix).exists() for folder in ('Frameworks', 'Libraries')):
                raise SystemExit(f'Unresolved bundled dependency in {p.relative_to(root)}: {dep}')
        if not dep.startswith(('@', '/System/Library/', '/usr/lib/')):
            raise SystemExit(f'Unbundled dependency in {p.relative_to(root)}: {dep}')
print(f'Standalone dependency check passed: {count} Mach-O files')
PY

version_output=$(QT_QPA_PLATFORM=cocoa "$core" --version)
if [[ "$version_output" != *"($short_revision)"* ]]; then
  echo 'Embedded revision does not match committed HEAD.' >&2
  exit 1
fi
archive="$package_dir/Deskflow-$short_revision.zip"
ditto -c -k --sequesterRsrc --keepParent "$bundle" "$archive"
{
  printf 'source_revision=%s\narchitecture=%s\n' "$revision" "$(uname -m)"
  printf 'scope=prepared only; no live installation or native input acceptance\n'
  printf 'signing=ad-hoc; not notarized\n'
  printf '%s\n' "$version_output"
  shasum -a 256 "$core" "$archive"
} > "$package_dir/receipt.txt"
printf '\nPrepared: %s\nReceipt: %s\n' "$bundle" "$package_dir/receipt.txt"
