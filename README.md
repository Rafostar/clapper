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
Clapper uses `GStreamer` bindings from `GI` repository, so if your repo ships them as separate package, they must be installed first.
Additionally Clapper requires these `GStreamer` elements:
* [gtkglsink](https://gstreamer.freedesktop.org/documentation/gtk/gtkglsink.html)
* [glsinkbin](https://gstreamer.freedesktop.org/documentation/opengl/glsinkbin.html)

Other required plugins (codecs) depend on video format.

## Installation
Run in terminal:
```shell
meson builddir --prefix=/usr/local
sudo meson install -C builddir
```

## Packages
The [pkgs folder](https://github.com/Rafostar/clapper/tree/master/pkgs) in this repository contains build scripts for various package formats.

### openSUSE, SLE & Fedora (rpm)
Prebuilt packagages are available here: [software.opensuse.org//download.html?project=home:sp1rit&package=clapper](https://software.opensuse.org//download.html?project=home%3Asp1rit&package=clapper) ([See status](https://build.opensuse.org/package/show/home:sp1rit/clapper))

### Arch Linux
You can get clapper from the AUR: [clapper-git](https://aur.archlinux.org/packages/clapper-git), or
```
cd pkgs/arch
makepkg -si
```

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
