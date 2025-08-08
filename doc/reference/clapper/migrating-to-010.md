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
      "own-name", mpris_name,
      "identity", APP_NAME,
      "desktop-entry", APP_ID, NULL);
  clapper_enhancer_proxy_set_target_creation_allowed (proxy, TRUE);
  gst_object_unref (proxy);
}
```

For more information how to use enhancers and how to write your own,
see [Clapper Enhancers](clapper-enhancers.html) documentation.
