# Clapper
A GNOME media player build using [GJS](https://gitlab.gnome.org/GNOME/gjs) with [GTK4](https://www.gtk.org) toolkit. The media player is using GStreamer [GstPlayer API](https://gstreamer.freedesktop.org/documentation/player/gstplayer) as a media backend and renders everything via [OpenGL](https://www.opengl.org). Can also be used as a pre-made widget for [GTK](https://www.gtk.org) apps.

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

### Features:
* [Hardware acceleration](https://github.com/Rafostar/clapper/wiki/Hardware-acceleration)
* [Floating mode](https://github.com/Rafostar/clapper/wiki/Floating-mode)
* [Adaptive UI](https://raw.githubusercontent.com/Rafostar/clapper/master/media/screencast-mobile-ui.webm)
* [Playlist from file](https://github.com/Rafostar/clapper/wiki/Playlists)
* Chapters on progress bar

## Installation from Flatpak (recommended)
The `Flatpak` package includes all required dependencies and codecs. Additionally it also has a few patches, thus some functionalities work better (or are only available) in `Flatpak` version (until my changes are accepted upstream). It is provided as an easy "just install and use" solution.

```sh
flatpak install https://rafostar.github.io/flatpak/com.github.rafostar.Clapper.flatpakref
```

## Packages / Installation from source code
The [pkgs folder](https://github.com/Rafostar/clapper/tree/master/pkgs) in this repository contains build scripts for various package formats. You can use them to build package yourself or download one of pre-built packages:

#### Debian, Fedora, openSUSE & Ubuntu
Pre-built packages are available in [my repo](https://software.opensuse.org//download.html?project=home%3ARafostar&package=clapper) ([see status](https://build.opensuse.org/package/show/home:Rafostar/clapper)).

#### Arch Linux
You can get Clapper from the AUR: [clapper-git](https://aur.archlinux.org/packages/clapper-git)

**Important:** If you build `Clapper` from source code or install it using any other packaging system than `Flatpak`, you will still need some additional `GStreamer` elements and patches that are not upstreamed in `GStreamer` source code yet. The requirements and how to build from git source code are described in the [wiki](https://github.com/Rafostar/clapper/wiki#installation-from-source-code).

## Q&A
**Q:** Does using `GJS` negatively impact video performance?<br>
**A:** Absolutely not. `GJS` here is used to put together the GUI during startup.
It has nothing to do with video rendering. All used `GTK4` and `GStreamer` libraries are in C.
Even the custom video widget that I prepared for this player (based on original `GTK3` implementation) is a compiled C code.
All these libs are acting "on their own" and no function calls from `GJS` related to video decoding and rendering are performed during playback.

**Q:** What settings should I set to maximize performance?<br>
**A:** As of now, player works best on `Wayland` session. `Wayland` users might want to try enabling the experimental `vah264dec` plugin for improved performance (this plugin does not work on `Xorg` right now) for standard (8-bit) `H.264` videos. It can be enabled from inside player preferences dialog inside `Advanced -> GStreamer` tab using customizable `Plugin Ranking` feature. Since the whole app is rendered using your GPU, users of VERY weak GPUs might try to disable the "render window shadows" option to have more GPU power available for non-fullscreen video rendering.

## Special Thanks
Many thanks to [sp1ritCS](https://github.com/sp1ritCS) for creating and maintaining package build files.

Thanks a lot to all the people who are supporting the development with their anonymous donations through [Liberapay](https://liberapay.com/Clapper/). I :heart: U.
