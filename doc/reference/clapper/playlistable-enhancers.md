Title: Playlistable Enhancers
Slug: playlistable-enhancers

### Overview

[iface@Clapper.Playlistable] is an interface to implement playlist parsers.
Allows to expand Clapper library with an ability to read data from which one
or more media items should be created.

To load playlist within Clapper, just create a new media item which has an URI
leading to data that playlistable enhancer will act upon. After parsing, that item
will be merged with first parsed item and the rest will be inserted into queue after
its position.

Essentially, such enhancer inserts items from a playlist into playback queue upon
which Clapper operates. It can also handle nested playlits (a playlist URI within
another playlist) with unlimited amount of nested levels.

For the basics about writing enhancers see [Clapper Enhancers](clapper-enhancers.html).

### Requirements

Additional fields for `.plugin` info file:

* `X-Data-Prefix` - describe text that data should start with
* `X-Data-Contains` - data must contain given phrase
* `X-Data-Regex` - regular expression to run on data

These are used by `typefinder` to determine whether given data is a playlist for
this enhancer to handle. At least one of the above must be added to plugin info file.

An enhancer of this type must implement [vfunc@Clapper.Playlistable.parse] virtual method.

### Parsing data

When [vfunc@Clapper.Playlistable.parse] is called, an empty [class@Gio.ListStore] is
passed as this function argument. Implementation is responsible for parsing data, creating
[class@Clapper.MediaItem] objects and adding them to that list store. While doing so, it
can also populate each media item tags, timeline markers and/or set subtitle URI.
