# GtkPlayer
A pre-made GJS Media Player widget powered by GStreamer with OpenGL rendering.

Required GStreamer elements:
*  [gtkglsink](https://gstreamer.freedesktop.org/documentation/gtk/gtkglsink.html)
*  [glsinkbin](https://gstreamer.freedesktop.org/documentation/opengl/glsinkbin.html)

Other required plugins (codecs) depend on video format.

To use VAAPI make sure you have `gstreamer1-vaapi` installed. Verify with:
```shell
gst-inspect-1.0 vaapi
```
On some older GPUs you might need to export `GST_VAAPI_ALL_DRIVERS=1` environment variable.
