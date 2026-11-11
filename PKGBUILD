# Maintainer: Cesar <yugurta@gmail.com>
pkgname=ktorrentsearch
pkgver=1.0.0
pkgrel=1
pkgdesc="Buscador nativo multi-tracker sobre CachyOS UI (nova3) con integración directa en Transmission Qt"
arch=('x86_64')
url="https://github.com/cesarconte/ktorrentsearch"
license=('GPL-3.0-or-later')
depends=('qt6-base' 'qt6-declarative' 'ki18n' 'kcoreaddons' 'kio' 'python' 'transmission-qt')
makedepends=('cmake' 'extra-cmake-modules')
source=()
sha256sums=()

build() {
    cmake -B build -S "${startdir}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DBUILD_TESTING=ON
    cmake --build build -j"$(nproc)"
}

check() {
    ctest --test-dir build --output-on-failure
}

package() {
    DESTDIR="${pkgdir}" cmake --install build
}
