#!/usr/bin/env python3

import gi
gi.require_version('Adw', '1')
gi.require_version('Clapper', '0.0')
gi.require_version('ClapperGtk', '0.0')
gi.require_version('Gst', '1.0')
gi.require_version('Gtk', '4.0')
from gi.repository import Adw, Clapper, ClapperGtk, Gdk, Gst, Gtk

Clapper.init(None)

def on_level_message(player, msg, bars):
    left_bar, right_bar = bars
    if (peak := msg.get_structure().get_value("peak")):
        # Convert dB to linear while taking player volume into account.
        volume = player.props.volume;
        left_bar.set_value(min(pow(10.0, peak[0] / 20.0) * volume, 1.0))
        right_bar.set_value(min(pow(10.0, peak[1] / 20.0) * volume, 1.0))

def on_state_changed(player, pspec, bars):
    if (player.props.state < Clapper.PlayerState.PLAYING):
        for bar in bars:
            bar.set_value(0)

def on_activate(app):
    # Create our widgets.
    win = Gtk.ApplicationWindow(application=app, title='Clapper Audio Level Meter', default_width=400, default_height=96)
    audio = ClapperGtk.Audio()
    hbox = Gtk.Box(valign=Gtk.Align.CENTER, margin_start=8, margin_end=8, spacing=4)
    vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, valign=Gtk.Align.CENTER, hexpand=True, spacing=4)
    play_btn = ClapperGtk.TogglePlayButton()
    left_level = Gtk.LevelBar()
    right_level = Gtk.LevelBar()

    # Use GStreamer "level" element which measures volume peaks before audio output.
    audio.props.player.props.audio_sink = Gst.parse_bin_from_description(
        'level interval=25000000 post-messages=true ! autoaudiosink', True)

    # Connect to player detailed "message" signal to receive "level" messages.
    audio.props.player.connect('message::level', on_level_message, (left_level, right_level))

    # Listen for player state changes to empty bars fill when not playing.
    audio.props.player.connect('notify::state', on_state_changed, (left_level, right_level))

    # Add media for playback. First media item in queue will be automatically selected.
    item = Clapper.MediaItem(uri='https://www.soundhelix.com/examples/mp3/SoundHelix-Song-1.mp3')
    audio.props.player.props.queue.add_item(item)

    # Assemble window.
    vbox.append(left_level)
    vbox.append(right_level)
    hbox.append(play_btn)
    hbox.append(vbox)
    audio.set_child(hbox)
    win.set_child(audio)
    win.present()

    # Not too loud. Mind the ears.
    audio.props.player.props.volume = 0.7

    # Start playback.
    audio.props.player.play()

# Create a new application.
app = Adw.Application(application_id='com.example.ClapperAudioLevelMeter')
app.connect('activate', on_activate)

# Run the application.
app.run(None)
