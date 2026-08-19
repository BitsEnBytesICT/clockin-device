#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
curl_dev_dir="$project_dir/build-desktop/curl-dev"
curl_header="$curl_dev_dir/root/usr/include/x86_64-linux-gnu/curl/curl.h"

if [[ ! -f "$curl_header" ]]; then
    echo "+ Preparing local libcurl development headers (no sudo required)"
    mkdir -p "$curl_dev_dir"
    (
        cd "$curl_dev_dir"
        packages=(libcurl4-openssl-dev_*.deb)
        if [[ ! -f "${packages[0]}" ]]; then
            apt-get download libcurl4-openssl-dev
            packages=(libcurl4-openssl-dev_*.deb)
            if [[ ! -f "${packages[0]}" ]]; then
                echo "Could not download libcurl4-openssl-dev" >&2
                exit 1
            fi
        fi
        dpkg-deb -x "${packages[0]}" root
    )
fi

cmake -S "$project_dir" -B "$project_dir/build-desktop" \
    -DDESKTOP_SIM=ON \
    -DCMAKE_BUILD_TYPE=Debug
cmake --build "$project_dir/build-desktop" --parallel

exec "$project_dir/build-desktop/imgui_app" "$@"
