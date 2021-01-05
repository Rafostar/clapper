imports.gi.versions.Gtk = '4.0';

const Gtk = imports.gi.Gtk;
const Clapper = imports.clapper;

let app = new Gtk.Application({
    application_id: 'com.clapper.WidgetExample'
});

app.connect('activate', () => {
    let window = new Gtk.ApplicationWindow({
        application: app,
        title: 'Clapper Widget Example',
        width_request: 460,
        height_request: 390,
    });
    let box = new Gtk.Box({
        orientation: Gtk.Orientation.VERTICAL,
    });
    let label = new Gtk.Label({
        label: [
            'Clapper is used as a widget and placed below.',
            'Double click it to enter fullscreen!',
        ].join('\n')
    });
    let widget = new Clapper.Widget();

    window.bind('fullscreened', label, 'visible', GObject.BindingFlags.INVERT_BOOLEAN);
    window.connect('show', () => {
        let media = 'http://distribution.bbb3d.renderfarming.net/video/mp4/bbb_sunflower_1080p_30fps_normal.mp4';
        widget.player.set_media(media);
    });

    box.append(label);
    box.append(widget);

    window.set_child(box);
    window.present();
});
app.run([]);
