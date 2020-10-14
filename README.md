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
Clapper uses `OpenGL` based sinks, so when `VA-API` is available, both CPU and RAM usage is much lower. Especially if you have `gst-plugins-bad` 1.18+ with new `vah264dec` decoder which shares a single GL context with Clapper and uses DRM connection. If you have an AMD/Intel GPU, I highly recommend this new decoder.

To use `VA-API` with H.264 videos, make sure you have `gst-plugins-bad` 1.18+. For other codecs additionally install `gstreamer1-vaapi`. Verify with:
```shell
gst-inspect-1.0 vah264dec
gst-inspect-1.0 vaapi
```
On some older GPUs you might need to export `GST_VAAPI_ALL_DRIVERS=1` environment variable.

Other acceleration methods (supported by `GStreamer`) should also work, but I have not tested them due to lack of hardware.
</details>

## Requirements
Clapper uses GTK4 along with `GStreamer` bindings from `GI` repository, so if your distro ships them as separate package, they must be installed first.
Additionally Clapper requires these `GStreamer` elements:
* [gtk4glsink](https://gstreamer.freedesktop.org/documentation/gtk/gtkglsink.html)
* [glsinkbin](https://gstreamer.freedesktop.org/documentation/opengl/glsinkbin.html)

**Attention:** `gtk4glsink` is my own port of current GStreamer `gtkglsink` to GTK4. The element is not part of GStreamer yet (pending review). Fedora package is available in my OBS repository. It will be installed along with Clapper if you add my repo to `dnf` package manager. Otherwise you might want to build it yourself from [source code](https://gitlab.freedesktop.org/Rafostar/gst-plugins-good/-/tree/GTK4) of my gstreamer GTK4 branch.

Other required plugins (codecs) depend on video format.

Recommended additional packages you should install manually via package manager:
* `gstreamer-libav` - codecs required to play most videos
* `gstreamer-vaapi` - hardware acceleration

Please note that packages naming varies by distro.

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
Run in terminal:
```shell
meson builddir --prefix=/usr/local
sudo meson install -C builddir
```

GStreamer elements installation:
<details>
  <summary><b>Debian/Ubuntu</b></summary>

```shell
sudo apt install \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-gl \
  gstreamer1.0-gtk4 \
  gstreamer1.0-libav \
  gstreamer-vaapi
```
</details>

<details>
  <summary><b>Fedora</b></summary>

Enable RPM Fusion and run:
```shell
sudo dnf install \
  gstreamer1-plugins-base \
  gstreamer1-plugins-good \
  gstreamer1-plugins-good-gtk4 \
  gstreamer1-plugins-bad-free \
  gstreamer1-plugins-bad-free-extras \
  gstreamer1-libav \
  gstreamer1-vaapi
```
</details>

<details>
  <summary><b>openSUSE</b></summary>

```shell
sudo zypper install \
  gstreamer-plugins-base \
  gstreamer-plugins-good \
  gstreamer-plugins-good-gtk4 \
  gstreamer-plugins-bad \
  gstreamer-plugins-libav \
  gstreamer-plugins-vaapi
```
</details>

<details>
  <summary><b>Arch Linux</b></summary>

```shell
sudo pacman -S \
  gst-plugins-base \
  gst-plugins-good \
  gst-plugin-gtk4 \
  gst-plugins-bad-libs \
  gst-libav \
  gstreamer-vaapi
```
</details>

## Special Thanks
Many thanks to [sp1ritCS](https://github.com/sp1ritCS) for creating and maintaining package build files.
