Title: Clapper Enhancers
Slug: clapper-enhancers

### Overview

Clapper Enhancers are special plugins loaded through `libpeas` for Clapper library.
The idea is to enhance it (including applications that use Clapper) with new
capabilities that do stuff outside of `GStreamer` scope of things without increasing
number of dependencies of Clapper itself (especially for features that not everyone needs).

To avoid confusion with term "plugins" that `GStreamer` uses, the name "enhancers"
was choosen instead.

In addition to writing enhancers in pure `C`, they can be written in any programmable
language that is supported by `libpeas` and works with Clapper library bindings
(examples being `Python`, `GJS` and `Vala`). This makes it possible for individual users
to add their own custom functionalities that they want to use individually.

### Types

Clapper gives three interfaces for writing enhancers for different things, these are:

- [Extractable](extractable-enhancers.html)
- [Playlistable](playlistable-enhancers.html)
- [Reactable](reactable-enhancers.html)

### Basics

Each enhancer is a `libpeas` compatible module. As such it follows its requirements for
writing plugins and is a [class@GObject.Object] sublass implementing one or more of interfaces
that Clapper library provides.

Due to the plugins nature, written enhancer can be either submitted to the main [Clapper Enhancers
repository](https://github.com/Rafostar/clapper-enhancers) when it seems useful for more than a single
application. Alternatively, it can be shipped as part your application. Users can also write their
own/custom local enhancer plugins.

### Loading Enhancers

Clapper will try to lazy load enhancer modules, meaning they will not be loaded unless they are used.
As an additional safety precaution, enhancers can be disallowed from their instances being created with
[property@Clapper.EnhancerProxy:target-creation-allowed]. Enhancers that operate on-demand
(when supported URI is given) such as [iface@Clapper.Extractable] and [iface@Clapper.Playlistable]
are allowed (enabled) by default, while [iface@Clapper.Reactable] are not.

Environment variables:

* `CLAPPER_ENHANCERS_PATH` - override for default path where Clapper loads enhancers from
* `CLAPPER_ENHANCERS_EXTRA_PATH` - additional path to scan for enhancer plugins

While both allow to specify multiple directories (with `:` separation), applications and
users should mostly use extra path in order to add their own enhancers from non-standard
installation directory. They can override default path in case where they want to forbid
using stock enhancers for some reason.

### Enhancer Proxies

In order to create enhancers when needed and use them only within thread they were created in,
Clapper manages enhancer instances on its own. It gives applications proxy objects for
browsing and configuring them.

Only after Clapper library is initialized, you can get either the global (application scope) list
([class@Clapper.EnhancerProxyList]) of enhancers proxies with [func@Clapper.get_global_enhancer_proxies]
or player scope with [method@Clapper.Player.get_enhancer_proxies].

Properties set on the global list will carry over to all created player instances afterwards.
While these set on a list from a player are applied to enhancers created by this player only.
You can use that to your advantage to only allow creation of some type of enhancer in only some
player instances.

### Properties

Some enhancers might want to expose some configuration. For this cases they should install
[class@GObject.Object] properties in their class. They must only use fundamental types from the list below:

* boolean
* int
* uint
* double
* string

They should include one or more of [flags@Clapper.EnhancerParamFlags].
Properties in object can have `global`, `local`, neither or both flags at once.

The ones with [Clapper.EnhancerParamFlags.GLOBAL] are for user to configure. They should
be exposed (ideally in the UI) and used for things that make sense to be set for all
applications at once (e.g. supported media codec by user device, preferred language, etc.).

On the other hand, properties with [Clapper.EnhancerParamFlags.LOCAL] are for application scope
usage only. They never carry over to other apps, nor they can be set on global enhancers list.
Instead they are configured per player instance.

When property is neither `global` or `local` it is considered to be plugin internal property.
Clapper will never access them, as such they can be of any type (not limited to above list).
This also makes properties that are already installed in base classes of subclassed object
not appear in the UI.

When both flags are set, property will initially use globally set value by user while still
allowing application to override this value locally per player instance.

Extra flags like [Clapper.EnhancerParamFlags.FILEPATH] and [Clapper.EnhancerParamFlags.DIRPATH]
can be used (usually with `string` type of property) in order to let application know that this
field holds a file/directory path and allow it to present file selection dialog to the user instead
of text entry.

### Configuring Enhancers

Applications browse properties of enhancer via [method@Clapper.EnhancerProxy.get_target_properties]
which returns a list of [class@GObject.ParamSpec]. By inspecting their `flags` application can know
whether property is `global`, `local` or both.

Use [method@Clapper.EnhancerProxy.get_settings] to get a [class@Gio.Settings] with `global` properties
to read and write (note that only users should be able to change them, thus you might want to bind these
into some UI widgets for that). These can be (with user consent) set on either proxy from global proxies
list or the player one.

Use [method@Clapper.EnhancerProxy.set_locally] to set `local` properties. These are meant for applications
to freely customize as they please. Remember that you can only set them on a enhancer proxy object belonging
to some player instance and not the global one.

### Plugin Info File

An enhancer plugin should be placed within directory that includes its [Peas] plugin
description file. It should be a text file with a `.plugin` extension and contain at least
following content:

```
[Plugin]
Module=example_enhancer
Name=Example Enhancer
Description=This enhancer serves as an example
Version=0.0.1
```

* `Module` - module entry file name. It also acts as enhancer ID and should be unique for each.
It is recommended to use app/custom prefix in its name.
* `Name` - module friendly name for displaying in UI and such.
* `Description` - description to present to the user for what it does.
* `Version` - enhancer version. In order to lazy load enhancers, Clapper will cache each
enhancer data and olny reload it if version changes, so keep this always updated.

If module is written in interpretable programming language it must also contain `Loader` key
with interpreter name (e.g. `Loader=python`).

Some enhancer interfaces require additional fields to be put in this file. They are described
in the requirements of each one in their documentation pages that are listed in the
[Types section](clapper-enhancers.html#types).

### Adding Enhancers to Flatpak App

If you are using Clapper as part of a `Flatpak` application, you can get all the enhancers from their main repo as an extension
(info [here](https://github.com/flathub/com.github.rafostar.Clapper?tab=readme-ov-file#comgithubrafostarclapperenhancers)),
thus you do not need to build them yourself.
