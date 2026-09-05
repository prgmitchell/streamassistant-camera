#!/usr/bin/env bash
set -euo pipefail

# Use the OBS plugin template's CPack DEB and lib/share archive layout.
version="${1:?Usage: Package-Linux.sh <version>}"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[A-Za-z0-9.-]+)?$ ]] || {
  echo 'Invalid package version' >&2
  exit 1
}
project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$project_root"
output_base="streamassistant-camera-v${version}-linux-x86_64"
stage="$project_root/release/linux-stage"

cmake --install build_x86_64 --prefix "$stage"
cpack --config build_x86_64/CPackConfig.cmake -G DEB \
  -D "CPACK_PACKAGE_VERSION=$version" \
  -D "CPACK_PACKAGE_FILE_NAME=$output_base" \
  -D CPACK_DEBIAN_DEBUGINFO_PACKAGE=OFF
tar -cJf "release/${output_base}.tar.xz" -C "$stage" lib share
