# Clapper
A GNOME media player built using [GJS](https://gitlab.gnome.org/GNOME/gjs) and powered by [GStreamer](https://gstreamer.freedesktop.org) with [OpenGL](https://www.opengl.org) rendering. Can also be used as a pre-made widget for [GTK](https://www.gtk.org) apps.

### WORK IN PROGRESS
This is still early WIP. Many features are not implemented yet and quite a few are still unstable. Right now Clapper can only be launched from terminal, so if you want to test it, start it like this:
```shell
com.github.rafostar.Clapper "video.mp4"
```

### Playlists
Clapper can also open playlist files. Playlist file is a standard text file with a `.claps` file extension. It should contain a single filepath per line. The filepath can be either absolute or relative. Playlist can even contain HTTP links instead of filepaths. Here is an example how to easily create a playlist file inside your music directory:
```shell
ls *.mp3 > music.claps
```
Once you have a playlist, open it with Clapper like any other file:
```shell
com.github.rafostar.Clapper "music.claps"
```
And since the playlist is a normal text file with filepaths only, you can always edit it later in any text editor. Easy, right?

## Requirements
Clapper uses `GStreamer` bindings from `GI` repository, so if your distro ships them as separate package, they must be installed first.
Additionally Clapper requires these `GStreamer` elements:
* [gtkglsink](https://gstreamer.freedesktop.org/documentation/gtk/gtkglsink.html)
* [glsinkbin](https://gstreamer.freedesktop.org/documentation/opengl/glsinkbin.html)

Other required plugins (codecs) depend on video format.

Recommended additional packages you should install manually via package manager:
* `gstreamer-libav` - codecs required to play most videos
* `gstreamer-vaapi` - hardware acceleration

Please note that packages naming varies by distro.

## Installation
Run in terminal:
```shell
meson builddir --prefix=/usr/local
sudo meson install -C builddir
```

Additional GStreamer elements installation:
<details>
  <summary>Fedora</summary>

Enable RPM Fusion and run:
```shell
sudo dnf install \
  gstreamer1-plugins-base \
  gstreamer1-plugins-good-gtk \
  gstreamer1-libav \
  gstreamer1-vaapi
```
</details>

<details>
  <summary>openSUSE</summary>

```shell
sudo zypper install \
  gstreamer-plugins-base \
  gstreamer-plugins-good \
  gstreamer-plugins-libav \
  gstreamer-plugins-vaapi
```
</details>

<details>
  <summary>Arch Linux</summary>

```shell
sudo pacman -S \
  gst-plugins-base \
  gst-plugin-gtk \
  gst-libav \
  gstreamer-vaapi
```
</details>

## Packages
The [pkgs folder](https://github.com/Rafostar/clapper/tree/master/pkgs) in this repository contains build scripts for various package formats. You can use them to build package yourself or download one of pre-built packages:
<details>
  <summary>Fedora, openSUSE & SLE (rpm)</summary>
  
Pre-built packages are available here:<br>
[software.opensuse.org//download.html?project=home:sp1rit&package=clapper](https://software.opensuse.org//download.html?project=home%3Asp1rit&package=clapper) ([See status](https://build.opensuse.org/package/show/home:sp1rit/clapper))
</details>

<details>
<summary>Arch Linux</summary>
  
You can get clapper from the AUR: [clapper-git](https://aur.archlinux.org/packages/clapper-git), or
```shell
cd pkgs/arch
makepkg -si
```
</details>

## Hardware acceleration
Using hardware acceleration is highly recommended. As stated in `GStreamer` wiki:
```
In the case of OpenGL based elements, the buffers have the GstVideoGLTextureUploadMeta meta, which
efficiently copies the content of the VA-API surface into a GL texture.
```
Clapper uses `OpenGL` based sinks, so when `VA-API` is available, both CPU and RAM usage is much lower.

To use `VA-API` make sure you have `gstreamer1-vaapi` installed. Verify with:
```shell
gst-inspect-1.0 vaapi
```
On some older GPUs you might need to export `GST_VAAPI_ALL_DRIVERS=1` environment variable.

Other acceleration methods (supported by `GStreamer`) should also work, but I have not tested them due to lack of hardware.

## Special Thanks
Many thanks to [sp1ritCS](https://github.com/sp1ritCS) for creating and maintaining package build files.
