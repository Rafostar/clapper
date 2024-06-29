#!/usr/bin/env python3

import gi
gi.require_version('Adw', '1')
gi.require_version('Clapper', '0.0')
gi.require_version('ClapperGtk', '0.0')
gi.require_version('GLib', '2.0')
gi.require_version('Gtk', '4.0')
from gi.repository import Adw, Clapper, ClapperGtk, GLib, Gtk
import shutil

Clapper.init(None)

download_dir = GLib.build_filenamev([GLib.get_user_cache_dir(), "example_download_dir", None])
print('Using cache directory: {0}'.format(download_dir))

def on_download_complete(player, item, location):
    # Media downloaded. Data from this file is still used for current playback (including seeking).
    print('Download complete: {0} => {1}'.format(item.props.uri, location))

def on_activate(app):
    win = Gtk.ApplicationWindow(application=app, default_width=640, default_height=396)
    video = ClapperGtk.Video()
    controls = ClapperGtk.SimpleControls(fullscreenable=False)

    # Enable local storage caching and monitor it
    video.props.player.set_download_dir(download_dir)
    video.props.player.set_download_enabled(True)
    video.props.player.connect('download-complete', on_download_complete)

    # Configure playback
    video.props.player.props.queue.set_progression_mode(Clapper.QueueProgressionMode.CAROUSEL)
    video.props.player.set_autoplay(True)

    # Assemble window
    video.add_fading_overlay(controls)
    win.set_child(video)
    win.present()

    # Create and add media for playback
    item_1 = Clapper.MediaItem(uri='http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4')
    item_2 = Clapper.MediaItem(uri='http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/ElephantsDream.mp4')
    video.props.player.props.queue.add_item(item_1)
    video.props.player.props.queue.add_item(item_2)

# Create a new application
app = Adw.Application(application_id='com.example.ClapperDownloadCache')
app.connect('activate', on_activate)

# Run the application
app.run(None)

# Finally app should cleanup before exit. Possibly moving data to
# another dir if it wants to use it on next run and deleting what's
# left (so any unfinished downloads will also be removed).
print('Cleanup')
shutil.rmtree(download_dir)
