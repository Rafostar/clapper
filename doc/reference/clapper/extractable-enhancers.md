Title: Extractable Enhancers
Slug: extractable-enhancers

### Overview

[iface@Clapper.Extractable] is an interface to implement enhancers that need to
resolve given URI into data that `GStreamer` will be able to play. While not
limited to, main use-case is for media information extraction.

While implementer is free to write whole extraction by hand, he can alternatively
integrate some 3rd party library that does most of this work. In such case, extractable
enhancer is more of a wrapper for integrating said library. This is especially useful
if library that you want to use is written in different programming language than Clapper is.

For the basics about writing enhancers see [Clapper Enhancers](clapper-enhancers.html).

### Requirements

Additional fields for `.plugin` info file: `X-Schemes` and `X-Hosts`. The former one should
hold the `;` separated list of supported URI schemes, while the latter is for hostnames.

Example:

```
X-Schemes=https;expl
X-Hosts=example.com;other.example.com
```

With this in place, enhancer will be loaded and used for URIs that match combinations
of one of the schemes and hosts. The special rule is that when using some custom URI
scheme other than `http(s)`, `X-Hosts` can be skipped since such URI explicitly
says to use this module.

Considering all of the above, this enhancer would try to extract URIs like:

* `https://example.com/video_id=ABCD`
* `expl://video.it?id=ABCD`

It would not act however for:

* `https://video.it?id=ABCD`
* `qwerty://other.example.com/video_id=ABCD`

An enhancer of this type must implement [vfunc@Clapper.Extractable.extract] virtual method.

### Harvesting data

When [vfunc@Clapper.Extractable.extract] is called, newly made [class@Clapper.Harvest]
is passed as this function argument. Implementation is responsible for filling it with
data (see [method@Clapper.Harvest.fill]) that can be played. Such content can be either
a resolved URI to actual media file or some streaming manifest (like `HLS` or `DASH`).

Supported media types for single item include:

* `application/dash+xml` - DASH manifest
* `application/x-hls` - HLS manifest
* `text/x-uri` - direct media URI

During extract function, implementation might optionally add media tags such as title
(which will be merged with tags of [class@Clapper.MediaItem]) and extra HTTP request
headers if any are required to access this content.

### Multiple items extraction

It is possible to handle URIs which would normally return more than one media item to play.
Examples being playlists, search queries, related videos, etc.

This can be handled in two ways, depending on set media type:

* `text/uri-list` - playlist of URIs
* `application/clapper-playlist` - custom playlist format

If you use the first option, harvest should be filled with idividual URIs one per line.
Clapper will use its built-in URI list parser to create a media item for each URI and
place them in its playback queue. This is equivalent of creating [class@Clapper.MediaItem]
for each of these URIs without setting any tags in it initially.

The second option requires for this enhancer to also implement [iface@Clapper.Playlistable]
interface. In this case, you can fill harvest with ANY kind of data considering that
[vfunc@Clapper.Playlistable.parse] of your own enhancer will be used with the data you
passed. With this, you can add more info to media items such as extra tags, timeline markers
or subtitle URI. Useful if your extractor extracts both URIs and tags in one go.
