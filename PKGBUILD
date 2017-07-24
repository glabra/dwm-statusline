pkgname=dwm-statusline
pkgver=0.2
pkgrel=1
pkgdesc="small xsetroot-like utility for dwm status line"
url=""
arch=('i686' 'x86_64')
license=('MIT')
depends=('libxcb' 'alsa-lib')
source=(dwm-statusline.c
        Makefile)
md5sums=('b3a202f3c0067a02656d2102056766f6'
         'da5d416674914b05c377bcbf5f8e769f')

build() {
  cd "$srcdir"
  make
}

package() {
  cd "$srcdir"
  make PREFIX=/usr DESTDIR="$pkgdir" install
}
