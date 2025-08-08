Title: Migrating to ClapperGtk 0.10
Slug: migrating-to-010

### AV widget

Code of [class@ClapperGtk.Video] was split into a base class [class@ClapperGtk.Av] from which
[class@ClapperGtk.Video] and newly added [class@ClapperGtk.Audio] widgets are made.

This code split implies following changes:

* Properties `auto-inhibit`, `inhibited` and `player` were moved into AV base class, since these
are usually used without explicit need to specify object class they belong to, this change should
not affect most use-cases.
* Methods to get above properties are now in AV, but also left in video widget for compatibility purposes.
* Built-in widget actions starting with `video.` prefix are deprecated (also left for compatibility).
Implementations that used them are encouraged to use `av.` actions now. All actions from video widget were
ported to AV widget as they were, so updating your app should be as easy as changing this action prefix.

### Other Info

Above describes changes to GTK integration library, for the playback library
check out [other migration guide](../clapper/migrating-to-010.html).
