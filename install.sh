#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
# install.sh: Instalador de KTorrentSearch (instalación estricta en /usr)

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "==> Compilando KTorrentSearch (CachyOS UI / Qt6) para /usr..."
rm -rf "${PROJECT_DIR}/build"
cmake -B "${PROJECT_DIR}/build" -S "${PROJECT_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="/usr" \
    -DBUILD_TESTING=ON \
    -G Ninja

cmake --build "${PROJECT_DIR}/build" -j"$(nproc)"

echo "==> Ejecutando suite de tests (ctest)..."
ctest --test-dir "${PROJECT_DIR}/build" --output-on-failure

echo "==> Instalando estrictamente en /usr..."
sudo -n cmake --install "${PROJECT_DIR}/build" --prefix /usr

echo "==> Configurando Transmission Qt como gestor predeterminado de magnet: y .torrent..."
if command -v gio >/dev/null 2>&1 && [[ -f /usr/share/applications/transmission-qt.desktop ]]; then
    gio mime x-scheme-handler/magnet transmission-qt.desktop || true
    gio mime application/x-bittorrent transmission-qt.desktop || true
fi

if command -v kbuildsycoca6 >/dev/null 2>&1; then
    kbuildsycoca6 --noincremental >/dev/null 2>&1 || true
fi

echo "==> Instalación completada en /usr/bin/ktorrentsearch"
