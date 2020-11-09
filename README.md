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

### Features:
<details>
  <summary><b>Playlists</b></summary>

Clapper can open playlist files. Playlist file is a standard text file with a `.claps` file extension.
It should contain a single filepath per line. The filepath can be either absolute or relative.
Playlist can even contain HTTP links instead of filepaths.

Here is an example how to easily create a playlist file inside your music directory:
```shell
ls *.mp3 > music.claps
```
Once you have a playlist, open it with Clapper like any other file.
Since the playlist is a normal text file with filepaths only, you can always edit it later in any text editor or `echo` more media to it. Easy, right?
</details>

<details>
  <summary><b>Hardware acceleration</b></summary>

Using hardware acceleration is highly recommended. As stated in `GStreamer` wiki:
```
In the case of OpenGL based elements, the buffers have the GstVideoGLTextureUploadMeta meta, which
efficiently copies the content of the VA-API surface into a GL texture.
```
Clapper uses `OpenGL` based sinks, so when `VA-API` is available, both CPU and RAM usage is much lower. Especially with `gst-plugins-bad` 1.18+ and new `vah264dec` decoder which shares a single GL context with Clapper and uses DRM connection. If you have an AMD/Intel GPU and use Wayland session, I highly recommend enabling this new decoder in Clapper `Preferences->Advanced->GStreamer`.

Other acceleration methods (supported by `GStreamer`) should also work, but I have not tested them due to lack of hardware.
</details>

## Installation from flatpak (recommended)
The flatpak package includes all required dependencies and codecs. Additionally it also has a few patches, thus some funcionalities work better (or are only available) on flatpak version (until my changes are accepted upstream).

```sh
flatpak install https://rafostar.github.io/flatpak/com.github.rafostar.Clapper.flatpakref
```

## Packages
The [pkgs folder](https://github.com/Rafostar/clapper/tree/master/pkgs) in this repository contains build scripts for various package formats. You can use them to build package yourself or download one of pre-built packages:
<details>
  <summary><b>Debian, Fedora, openSUSE & Ubuntu</b></summary>
  
Pre-built packages are available in [my repo](https://software.opensuse.org//download.html?project=home%3ARafostar&package=clapper) ([see status](https://build.opensuse.org/package/show/home:Rafostar/clapper))
</details>

<details>
<summary><b>Arch Linux</b></summary>

You can get Clapper from the AUR: [clapper-git](https://aur.archlinux.org/packages/clapper-git), or
```shell
cd pkgs/arch
makepkg -si
```
</details>

## Installation from source code
The requirements and how to build from git source code are described in the [wiki](https://github.com/Rafostar/clapper/wiki#installation-from-source-code).

## Special Thanks
Many thanks to [sp1ritCS](https://github.com/sp1ritCS) for creating and maintaining package build files.
