pkgname=dwm-statusline
pkgver=0.1
pkgrel=1
pkgdesc="small xsetroot-like utility for dwm status line"
url=""
arch=('i686' 'x86_64')
license=('MIT')
depends=('libxcb' 'alsa-lib')
source=(dwm-statusline.c
        Makefile)
md5sums=('4387fb25f0f8659d70bcefe2eca8684b'
         'da5d416674914b05c377bcbf5f8e769f')

build() {
  cd "$srcdir"
  make
}

package() {
  cd "$srcdir"
  make PREFIX=/usr DESTDIR="$pkgdir" install
}
