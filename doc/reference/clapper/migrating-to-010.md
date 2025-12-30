Title: Migrating to Clapper 0.10
Slug: migrating-to-010

### Replace Features with Enhancers

Clapper 0.10 deprecates [class@Clapper.Feature] objects in favour [Clapper Enhancers](clapper-enhancers.html)
used via [class@Clapper.EnhancerProxy].

Old [class@Clapper.Feature] objects (including `mpris`, `discoverer` and `server`) are left for compatibility
reasons, but all apps using them are advised to migrate to enhancer plugins which already surpassed former
ones in what can be achieved with them.

Since these are in the form of plugins scanned during init, one of the differences is that you now check
their availability at runtime instead of compile time like before.

Something like this:

```c
#if CLAPPER_HAVE_MPRIS
  ClapperFeature *feature = CLAPPER_FEATURE (clapper_mpris_new (
      mpris_name, APP_NAME, APP_ID));
  clapper_player_add_feature (player, feature);
  gst_object_unref (feature);
#endif
```

Can be implemented like this:

```c
ClapperEnhancerProxyList *proxies = clapper_player_get_enhancer_proxies (player);
ClapperEnhancerProxy *proxy = clapper_enhancer_proxy_list_get_proxy_by_module (proxies, "clapper-mpris");

if (proxy) {
  clapper_enhancer_proxy_set_locally (proxy,
      "app-id", APP_ID,
      "own-name", mpris_name,
      "identity", APP_NAME,
      "desktop-entry", APP_ID, NULL);
  clapper_enhancer_proxy_set_target_creation_allowed (proxy, TRUE);
  gst_object_unref (proxy);
}
```

For more information how to use enhancers and how to write your own,
see [Clapper Enhancers](clapper-enhancers.html) documentation.

If you still want to support both deprecated and new enhancer based implementations for the time being,
you can always add an `else` code block for when `proxy` is not found and move your old logic there.

### Enhancers within Flatpak

If you are developing/maintaining a Flatpak application, it is recommended to build Clapper without above 3 deprecated
features (disable them through `meson`) as you can get enhancer plugins in the form of an extension. If you have not
done that already, check out [Clapper Flathub repo](https://github.com/flathub/com.github.rafostar.Clapper) for info
how to add them to your application manifest.

### Checking Enhancers

The [func@Clapper.enhancer_check] function gets deprecated. It was originally designed when only `Extractable` enhancers
existed. Now with more robust plugin system that simply allows more then just handling certain `scheme` and `host` this
function does not fit in it, hence its deprecation.

In most cases, applications should follow normal workflow, that is play something and if it does not work, handle an error
(with possible fallback action). For cases where an app wants to inspect available enhancers and their capabilities, it can
iterate (or get by module name) from [class@Clapper.EnhancerProxyList] proxies (either from player or global list) and inspect
its plugin info using given proxy object.

An easy to use replacement for [func@Clapper.enhancer_check] with proxies that implement [iface@Clapper.Extractable] interface,
would be [method@Clapper.EnhancerProxy.extra_data_lists_value] with `X-Schemes` and `X-Hosts` as keys.

### Tags Injection Alternative

This version adds a [method@Clapper.MediaItem.populate_tags] which allows to set an item tag list initially.
Useful for applications that know additional media item metadata that may not be inside its container or
if they want to set some initial values to be displayed in UI before player will update them.

Previously the sole method to do something like that was to use `taginject` element in pipeline (e.g. as player
`video-filter`). That method is still valid and supported, but trickier to perform as it requires app to synchronize
values for another media item at the right moment (stop, change item, set new tag list, play).

Basically, use `taginject` if you want to force override for tags of media item. If you want to just set some
initial values to ensure they never are empty, use [method@Clapper.MediaItem.populate_tags] instead.
