Format: 3.0 (quilt)
Source: clapper
Binary: clapper
Architecture: any
Version: 0.2.1
Maintainer: Rafostar <rafostar.github@gmail.com>
Build-Depends: debhelper (>= 10),
               meson (>= 0.50),
               gjs,
               gobject-introspection,
               libgtk-4-dev (>= 4.0.0),
               libgstreamer1.0-dev (>= 1.18),
               libgstreamer-plugins-base1.0-dev (>= 1.18),
               libgstreamer-gl1.0-0 (>= 1.18),
               libgles-dev,
               libglib2.0-dev,
               libglib2.0-bin,
               desktop-file-utils,
               hicolor-icon-theme,
               brz,
               libfontconfig1-dev,
               libpam-systemd
Package-List:
 clapper deb gnome optional arch=any
Files:
 0 0 debian.tar.xz
Description: Simple and modern GNOME media player
 A GNOME media player built using GJS with GTK4 toolkit and powered by GStreamer with OpenGL rendering.
