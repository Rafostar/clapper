#!/usr/bin/env python3

import gi
gi.require_version('Adw', '1')
gi.require_version('Clapper', '0.0')
gi.require_version('ClapperGtk', '0.0')
gi.require_version('Gtk', '4.0')
from gi.repository import Adw, Clapper, ClapperGtk, Gtk

Clapper.init(None)

def on_activate(app):
    # Create our widgets.
    win = Gtk.ApplicationWindow(application=app, title='Clapper Audio', default_width=640, default_height=96)
    audio = ClapperGtk.Audio()
    box = Gtk.Box(valign=Gtk.Align.CENTER, margin_start=8, margin_end=8, spacing=4)
    prev_btn = ClapperGtk.PreviousItemButton()
    play_btn = ClapperGtk.TogglePlayButton()
    next_btn = ClapperGtk.NextItemButton()
    seek_bar = ClapperGtk.SeekBar()

    # Add media for playback. First media item in queue will be automatically selected.
    item = Clapper.MediaItem(uri='https://www.soundhelix.com/examples/mp3/SoundHelix-Song-1.mp3')
    audio.props.player.props.queue.add_item(item)

    item = Clapper.MediaItem(uri='https://www.learningcontainer.com/wp-content/uploads/2020/02/Kalimba.mp3')
    audio.props.player.props.queue.add_item(item)

    # Assemble window.
    box.append(prev_btn)
    box.append(play_btn)
    box.append(next_btn)
    box.append(seek_bar)
    audio.set_child(box)
    win.set_child(audio)
    win.present()

    # Not too loud. Mind the ears.
    audio.props.player.props.volume = 0.7

    # Start playback.
    audio.props.player.play()

# Create a new application.
app = Adw.Application(application_id='com.example.ClapperAudio')
app.connect('activate', on_activate)

# Run the application.
app.run(None)
