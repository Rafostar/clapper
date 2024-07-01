# Clapper
[![Flatpak](https://github.com/Rafostar/clapper/actions/workflows/flatpak.yml/badge.svg?event=push)](https://github.com/Rafostar/clapper/actions/workflows/flatpak.yml)
[![Flatpak Nightly](https://github.com/Rafostar/clapper/actions/workflows/flatpak-nightly.yml/badge.svg?event=schedule)](https://github.com/Rafostar/clapper/actions/workflows/flatpak-nightly.yml)
[![Crowdin](https://badges.crowdin.net/clapper/localized.svg)](https://crowdin.com/project/clapper)
[![Matrix](https://img.shields.io/matrix/clapper-player:matrix.org?label=matrix)](https://matrix.to/#/#clapper-player:matrix.org)
[![Donate](https://img.shields.io/liberapay/receives/Clapper.svg?logo=liberapay)](https://liberapay.com/Clapper)

Clapper is a modern media player designed for simplicity and ease of use. Powered by [GStreamer](https://gstreamer.freedesktop.org/) and built for the GNOME
desktop environment using [GTK4](https://www.gtk.org/) toolkit, it has a clean and stylish interface that lets you focus on enjoying your favorite videos.

This application aim is to offer all the essentials features you'd expect from a video player in a simple form.

<p align="center">
  <img src="https://raw.githubusercontent.com/wiki/Rafostar/clapper/media/screenshot_01.png">
</p>

Clapper uses a playback queue where you can add multiple media files. Think of it like a playlist that you can build.
You can easily reorder items or remove them from the queue with a simple drag and drop operation.

<p align="center">
  <img src="https://raw.githubusercontent.com/wiki/Rafostar/clapper/media/screenshot_03.png">
</p>

### Components
Clapper's codebase consists of 2 libraries using which main application is built:
* [Clapper](https://rafostar.github.io/clapper/doc/clapper/) - a playback library
* [ClapperGtk](https://rafostar.github.io/clapper/doc/clapper-gtk/) - a GTK integration library

Both libraries support *GObject Introspection* bindings. A simple application example can be found [here](https://github.com/Rafostar/clapper-vala-test).

Above libraries are licensed under `LGPL-2.1-or-later`. You are free to use them in your own projects as long as you comply with license terms.
Please note that until version 1.0 they should be considered as an unstable API (some things may change without prior notice).

Clapper `Vala` bindings are part of this repo, while `Rust` bindings can be found [here](https://gitlab.gnome.org/JanGernert/clapper-rs).

## Installation from Flatpak
The `Flatpak` package includes all required dependencies and codecs.
Additionally it also has a few patches, thus some functionalities work better in `Flatpak` version (until my changes are accepted upstream).
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
meson setup builddir
cd builddir
meson compile
sudo meson install
```

If you want to compile app as `Flatpak`, remember to clone this repo with `--recurse-submodules` option.

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
Also words of appreciation for [JanGernert](https://gitlab.gnome.org/JanGernert) who made and is sharing Clapper Rust bindings.

Thanks a lot to all the people who are supporting the development with their anonymous donations through [Liberapay](https://liberapay.com/Clapper/). I :heart: U.
