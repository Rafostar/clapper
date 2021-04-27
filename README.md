# Clapper
A GNOME media player build using [GJS](https://gitlab.gnome.org/GNOME/gjs) with [GTK4](https://www.gtk.org) toolkit.
The media player is using [GStreamer](https://gstreamer.freedesktop.org/) as a media backend and renders everything via [OpenGL](https://www.opengl.org).

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

## Installation from Flatpak
The `Flatpak` package includes all required dependencies and codecs.
Additionally it also has a few patches, thus some functionalities work better (or are only available) in `Flatpak` version (until my changes are accepted upstream). List of patches used in this version can be found [here](https://github.com/Rafostar/clapper/issues/35).

<a href='https://flathub.org/apps/details/com.github.rafostar.Clapper'><img width='240' alt='Download on Flathub' src='https://flathub.org/assets/badges/flathub-badge-en.png'/></a>

**Important:** If you have been using the flatpak package from my custom 3rd party repo, please remove it and replace your installation with version from Flathub. That repository will not be maintained any longer. Thank you for understanding.

## Packages
The [pkgs folder](https://github.com/Rafostar/clapper/tree/master/pkgs) in this repository contains build scripts for various package formats. You can use them to build package yourself or download one of pre-built packages:

#### Debian, Fedora & openSUSE
Pre-built packages are available in [my repo](https://software.opensuse.org//download.html?project=home%3ARafostar&package=clapper) ([see status](https://build.opensuse.org/package/show/home:Rafostar/clapper)).<br>
Those are automatically build on each git commit, thus are considered unstable.

#### Arch Linux
You can get Clapper from the AUR:
* [clapper](https://aur.archlinux.org/packages/clapper) (stable version)
* [clapper-git](https://aur.archlinux.org/packages/clapper-git)

## Installation from source code
```sh
meson builddir --prefix=/usr/local
sudo meson install -C builddir
```

## Q&A
**Q:** Does using `GJS` negatively impact video performance?<br>
**A:** Absolutely not. `GJS` here is used to put together the GUI during startup.
It has nothing to do with video rendering. All used `GTK4` and `GStreamer` libraries are in C.
Even the custom video widget that I prepared for this player (based on original `GTK3` implementation) is a compiled C code.
All these libs are acting "on their own" and no function calls from `GJS` related to video decoding and rendering are performed during playback.

**Q:** What settings should I set to maximize performance?<br>
**A:** As of now, player works best on `Wayland` session. `Wayland` users can try enabling highly experimental `vah264dec` plugin for improved performance (this plugin does not work on `Xorg` right now) for standard (8-bit) `H.264` videos.
It can be enabled from inside player preferences dialog inside `Advanced -> GStreamer` tab using customizable `Plugin Ranking` feature.
Since the whole app is rendered using your GPU, users of VERY weak GPUs might want to disable the "render window shadows" option to have more GPU power available for non-fullscreen video rendering.

## Other Questions?
Feel free to ask me any questions. Come and talk on Matrix: [#clapper-player:matrix.org](https://matrix.to/#/#clapper-player:matrix.org)

## Special Thanks
Many thanks to [sp1ritCS](https://github.com/sp1ritCS) for creating and maintaining package build files.

Thanks a lot to all the people who are supporting the development with their anonymous donations through [Liberapay](https://liberapay.com/Clapper/). I :heart: U.
