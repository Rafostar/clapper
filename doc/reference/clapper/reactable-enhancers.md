Title: Reactable Enhancers
Slug: reactable-enhancers

### Overview

[iface@Clapper.Reactable] is an interface to implement enhancers that react to the
playback and/or events that should influence it.

Such enhancer can work not only in a way that triggers external actions due to some
playback events, but also in reverse - alters playback or its queue due to some
external event. It can do so by getting a hold of the player that given enhancer
instance reacts to with [method@Clapper.Reactable.get_player].

For the basics about writing enhancers see [Clapper Enhancers](clapper-enhancers.html).

### Requirements

An enhancer of this type should implement any of the [iface@Clapper.Reactable] virtual
methods that it needs.

Note that when implementing [vfunc@Clapper.Reactable.queue_item_removed] you probably
also want to implement [vfunc@Clapper.Reactable.queue_cleared] as the former is not
called for each item when clearing the whole queue for performance reasons.
