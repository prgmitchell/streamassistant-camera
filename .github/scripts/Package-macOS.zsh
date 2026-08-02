#!/usr/bin/env zsh

set -euo pipefail

if (( $# != 1 )) || [[ -z ${1} ]]; then
	print -u2 "Usage: ${0:t} <version>"
	exit 2
fi

readonly package_version=${1}
readonly project_root=${0:A:h:h:h}
readonly release_root=${project_root}/release
readonly bundle=${release_root}/RelWithDebInfo/streamassistant-camera.plugin
readonly installer=${release_root}/RelWithDebInfo/streamassistant-camera.pkg
readonly output_base=${release_root}/streamassistant-camera-v${package_version}-macos-universal

if [[ ! -d ${bundle} || ! -f ${installer} ]]; then
	print -u2 "Installed plugin bundle or package not found."
	exit 1
fi

ditto -c -k --sequesterRsrc --keepParent "${bundle}" "${output_base}.zip"
ditto "${installer}" "${output_base}.pkg"
