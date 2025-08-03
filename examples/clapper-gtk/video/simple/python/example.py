#!/usr/bin/env python3

import gi
gi.require_version('Adw', '1')
gi.require_version('Clapper', '0.0')
gi.require_version('ClapperGtk', '0.0')
gi.require_version('Gtk', '4.0')
from gi.repository import Adw, Clapper, ClapperGtk, Gtk

Clapper.init(None)

def on_toggle_fullscreen(video, win):
    # Since this example uses only video inside normal window, all we
    # need to toggle fullscreen is to invert its fullscreened property
    win.props.fullscreened ^= True

def on_activate(app):
    win = Gtk.ApplicationWindow(application=app, default_width=640, default_height=396)
    video = ClapperGtk.Video()
    controls = ClapperGtk.SimpleControls()

    # This signal will be emitted when user requests fullscreen state change.
    # It is app job to fullscreen video only (which might require something
    # more than simply inverting fullscreen on the whole window).
    video.connect('toggle-fullscreen', on_toggle_fullscreen, win)

    # Create and add media for playback. First added media item to empty
    # playback queue will be automatically selected.
    item = Clapper.MediaItem(uri='http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4')
    video.props.player.props.queue.add_item(item)

    # Assemble window
    video.add_fading_overlay(controls)
    win.set_child(video)
    win.present()

    # Start playback
    video.props.player.play()

# Create a new application
app = Adw.Application(application_id='com.example.ClapperSimple')
app.connect('activate', on_activate)

# Run the application
app.run(None)
