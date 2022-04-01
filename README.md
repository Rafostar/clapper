# Clapper
[![Flatpak](https://github.com/Rafostar/clapper/actions/workflows/flatpak.yml/badge.svg?event=push)](https://github.com/Rafostar/clapper/actions/workflows/flatpak.yml)
[![Flatpak Nightly](https://github.com/Rafostar/clapper/actions/workflows/flatpak-nightly.yml/badge.svg?event=schedule)](https://github.com/Rafostar/clapper/actions/workflows/flatpak-nightly.yml)
[![Crowdin](https://badges.crowdin.net/clapper/localized.svg)](https://crowdin.com/project/clapper)
[![Matrix](https://img.shields.io/matrix/clapper-player:matrix.org?label=matrix)](https://matrix.to/#/#clapper-player:matrix.org)
[![Donate](https://img.shields.io/liberapay/receives/Clapper.svg?logo=liberapay)](https://liberapay.com/Clapper)

A GNOME media player built using [GJS](https://gitlab.gnome.org/GNOME/gjs) with [GTK4](https://www.gtk.org) toolkit.
The media player uses [GStreamer](https://gstreamer.freedesktop.org/) as a media backend and renders everything via [OpenGL](https://www.opengl.org).

<p align="center">
<img src="https://raw.githubusercontent.com/wiki/Rafostar/clapper/media/screenshot-windowed.png"><br>
  <b>Windowed Mode</b>
</p>

<p align="center">
<img src="https://raw.githubusercontent.com/wiki/Rafostar/clapper/media/screenshot-fullscreen.png"><br>
  <b>Fullscreen Mode</b>
</p>

<p align="center">
<img src="https://raw.githubusercontent.com/wiki/Rafostar/clapper/media/screenshot-floating.png"><br>
  <b>Floating Mode</b>
</p>

### Features:
* [Hardware acceleration](https://github.com/Rafostar/clapper/wiki/Hardware-acceleration)
* [Floating mode](https://github.com/Rafostar/clapper/wiki/Floating-mode)
* [Adaptive UI](https://raw.githubusercontent.com/wiki/Rafostar/clapper/media/screenshot-mobile.png)
* [Playlist from file](https://github.com/Rafostar/clapper/wiki/Playlists)
* Chapters on progress bar
* MPRIS support

## Installation from Flatpak
The `Flatpak` package includes all required dependencies and codecs.
Additionally it also has a few patches, thus some functionalities work better (or are only available) in `Flatpak` version (until my changes are accepted upstream).
List of patches used in this version can be found [here](https://github.com/Rafostar/clapper/issues/35).

<a href='https://flathub.org/apps/details/com.github.rafostar.Clapper'>
  <img width='240' alt='Download on Flathub' src='https://flathub.org/assets/badges/flathub-badge-en.png'/>
</a>

## Packages in Linux Distributions
[![Packaging status](https://repology.org/badge/vertical-allrepos/clapper.svg)](https://repology.org/project/clapper/versions)

Pre-built RPM packages are also available in [my repo](https://software.opensuse.org//download.html?project=home%3ARafostar&package=clapper) ([see status](https://build.opensuse.org/package/show/home:Rafostar/clapper)).<br>
Those are automatically built on each git commit, thus are considered unstable.

## Installation from Source Code
```sh
meson builddir --prefix=/usr/local
sudo meson install -C builddir
```

## Questions?
Feel free to ask me any questions. Come and talk on Matrix: [#clapper-player:matrix.org](https://matrix.to/#/#clapper-player:matrix.org)

## Translations
Preferred translation method is to use [Clapper Crowdin](https://crowdin.com/project/clapper) web page.

Crowdin does not require any additional tools and translating can be done through web browser.
You can login using GitHub account or create a new one. Only I can add new languages to this project,
so if your language is not available, please contact me first.

## Special Thanks
Many thanks to [sp1ritCS](https://github.com/sp1ritCS) for creating and maintaining package build files.
Big thanks to [bridadan](https://github.com/bridadan) and [Uniformbuffer3](https://github.com/Uniformbuffer3) for helping
with testing V4L2 and NVDEC hardware acceleration methods.

Thanks a lot to all the people who are supporting the development with their anonymous donations through [Liberapay](https://liberapay.com/Clapper/). I :heart: U.
