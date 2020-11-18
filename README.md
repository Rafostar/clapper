# Clapper
A GNOME media player built using [GJS](https://gitlab.gnome.org/GNOME/gjs) and powered by [GStreamer](https://gstreamer.freedesktop.org) with [OpenGL](https://www.opengl.org) rendering. Can also be used as a pre-made widget for [GTK](https://www.gtk.org) apps.

<p align="center">
<img src="https://github.com/Rafostar/clapper/raw/master/media/screenshot-windowed-mode.png"><br>
  <b>Windowed Mode</b>
</p>

<p align="center">
<img src="https://github.com/Rafostar/clapper/raw/master/media/screenshot-fullscreen-mode.png"><br>
  <b>Fullscreen Mode</b>
</p>

<p align="center">
<img src="https://github.com/Rafostar/clapper/raw/master/media/screenshot-floating-mode.png"><br>
  <b>Floating Mode</b>
</p>

### WORK IN PROGRESS
This is still early WIP. Many features are not implemented yet and quite a few are still unstable.

Clapper is a GNOME media player build using [GJS](https://gitlab.gnome.org/GNOME/gjs) with [GTK4](https://www.gtk.org) toolkit. The media player is using GStreamer [GstPlayer API](https://gstreamer.freedesktop.org/documentation/player/gstplayer) as a media backend and renders everything via [OpenGL](https://www.opengl.org). Both [GTK4](https://www.gtk.org) and [GStreamer](https://gstreamer.freedesktop.org) plugins share a single global GL context which improves performance greatly.

### Features:
* [Playlists](https://github.com/Rafostar/clapper/wiki/Playlists)
* [Hardware acceleration](https://github.com/Rafostar/clapper/wiki/Hardware-acceleration)
* [Floating mode](https://github.com/Rafostar/clapper/wiki/Floating-mode)

## Installation from Flatpak (recommended)
The flatpak package includes all required dependencies and codecs. Additionally it also has a few patches, thus some functionalities work better (or are only available) in flatpak version (until my changes are accepted upstream).

```sh
flatpak install https://rafostar.github.io/flatpak/com.github.rafostar.Clapper.flatpakref
```

## Packages
The [pkgs folder](https://github.com/Rafostar/clapper/tree/master/pkgs) in this repository contains build scripts for various package formats. You can use them to build package yourself or download one of pre-built packages:

#### Debian, Fedora, openSUSE & Ubuntu
Pre-built packages are available in [my repo](https://software.opensuse.org//download.html?project=home%3ARafostar&package=clapper) ([see status](https://build.opensuse.org/package/show/home:Rafostar/clapper)).

#### Arch Linux
You can get Clapper from the AUR: [clapper-git](https://aur.archlinux.org/packages/clapper-git)

## Installation from source code
The requirements and how to build from git source code are described in the [wiki](https://github.com/Rafostar/clapper/wiki#installation-from-source-code).

## Special Thanks
Many thanks to [sp1ritCS](https://github.com/sp1ritCS) for creating and maintaining package build files.
