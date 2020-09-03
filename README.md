# Clapper
A GNOME media player built using [GJS](https://gitlab.gnome.org/GNOME/gjs) and powered by [GStreamer](https://gstreamer.freedesktop.org) with [OpenGL](https://www.opengl.org) rendering. Can also be used as a pre-made widget for [GTK](https://www.gtk.org) apps.

<b>WORK IN PROGRESS</b>

## Requirements
Clapper uses `GStreamer` bindings from `GI` repository, so if your repo ships them as separate package, they must be installed first.
Additionally Clapper requires these `GStreamer` elements:
* [gtkglsink](https://gstreamer.freedesktop.org/documentation/gtk/gtkglsink.html)
* [glsinkbin](https://gstreamer.freedesktop.org/documentation/opengl/glsinkbin.html)

Other required plugins (codecs) depend on video format.

## Installation
Run in terminal:
```sh
sudo ./install.sh
```
I know that this should be done using some sort of build system (like `meson`), but the player is still far from finished and a basic install script should be sufficient for the time being, if anyone wishes to test it.

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
