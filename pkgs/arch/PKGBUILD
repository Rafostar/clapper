#
# PKGBUILD file for package clapper
#
# Copyright (C) 2020  sp1rit
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# Maintainer: sp1rit <sp1ritCS@protonmail.com>

pkgname=clapper-git
pkgver=0.0.0
pkgrel=1
pkgdesc="A GNOME media player built using GJS and powered by GStreamer with OpenGL rendering. Can also be used as a pre-made widget for Gtk apps."
arch=(any)
url="https://github.com/Rafostar/clapper"
license=("GPL-3.0")
depends=("gjs" "gst-plugins-base-libs" "gst-plugin-gtk" "gst-plugins-bad-libs")
makedepends=("meson" "gjs")
optdepends=("gst-libav: Additional Codecs", "gstreamer-vaapi: Hardware acceleration")
provides=("${pkgname%-git}")
source=("${pkgname%-git}-$pkgver"::git+https://github.com/Rafostar/clapper.git)
md5sums=("SKIP")

prepare() {
	cd "${pkgname%-git}-$pkgver"
}

build() {
	cd "${pkgname%-git}-$pkgver"
	meson build/ --prefix=/usr
}

package() {
	cd "${pkgname%-git}-$pkgver"
	DESTDIR="$pkgdir" meson install -C build/
	ln -s "$pkgdir/usr/bin/com.github.rafostar.Clapper" "$pkgdir/usr/bin/clapper"
}

