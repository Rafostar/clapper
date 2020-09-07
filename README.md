# Clapper
A GNOME media player built using [GJS](https://gitlab.gnome.org/GNOME/gjs) and powered by [GStreamer](https://gstreamer.freedesktop.org) with [OpenGL](https://www.opengl.org) rendering. Can also be used as a pre-made widget for [GTK](https://www.gtk.org) apps.

### WORK IN PROGRESS
This is still early WIP. Many features are not implemented yet and quite a few are still unstable. Right now Clapper can only play single file from URI. So if you want to test it, start it from terminal like this:
```shell
clapper file:///path/to/video.mkv
```
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

## Performace Comparison
Here is the average **CPU** and **RAM** usage (lower is better) when playing the same H.264 1080p video in Clapper and [Totem](https://wiki.gnome.org/Apps/Videos) (GNOME Videos) with **VA-API enabled** on an old AMD APU:

| Player  | CPU |  RAM  |
| ------- | --- | ----- |
| Clapper |  3% | 52MB  |
| Totem   | 12% | 124MB |
