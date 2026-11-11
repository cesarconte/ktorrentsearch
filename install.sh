#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${HOME}/.local"

echo "==> Compilando KTorrentSearch (CachyOS UI / Qt6)..."
cmake -B "${PROJECT_DIR}/build" -S "${PROJECT_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
    -DBUILD_TESTING=ON

cmake --build "${PROJECT_DIR}/build" -j"$(nproc)"

echo "==> Ejecutando suite de tests (ctest)..."
ctest --test-dir "${PROJECT_DIR}/build" --output-on-failure

echo "==> Instalando en ${PREFIX}..."
cmake --install "${PROJECT_DIR}/build"

echo "==> Configurando Transmission Qt como gestor predeterminado de magnet: y .torrent..."
if command -v gio >/dev/null 2>&1 && [[ -f /usr/share/applications/transmission-qt.desktop ]]; then
    gio mime x-scheme-handler/magnet transmission-qt.desktop || true
    gio mime application/x-bittorrent transmission-qt.desktop || true
fi

update-desktop-database "${PREFIX}/share/applications" 2>/dev/null || true
echo "==> Instalación completada: ${PREFIX}/bin/ktorrentsearch"
