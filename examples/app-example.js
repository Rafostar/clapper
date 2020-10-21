imports.gi.versions.Gtk = '4.0';

const Gtk = imports.gi.Gtk;
const Clapper = imports.clapper;

let app = new Gtk.Application({
    application_id: 'com.clapper.AppExample'
});

app.connect('activate', () => {
    let window = new Gtk.ApplicationWindow({
        application: app,
        title: 'Clapper App Example',
    });
    let grid = new Gtk.Grid({
        halign: Gtk.Align.CENTER,
        valign: Gtk.Align.CENTER,
        margin_start: 20,
        margin_end: 20,
        margin_top: 10,
        margin_bottom: 10,
    });
    let box = new Gtk.Box({
        orientation: Gtk.Orientation.HORIZONTAL,
        spacing: 8,
    });
    let button = new Gtk.Button({
        icon_name: 'folder-videos-symbolic',
    });
    let label = new Gtk.Label({
        label: 'Click this button to play Big Buck Bunny!',
    });

    button.connect('clicked', () => {
        let clapper = new Clapper.App({
            playlist: ['http://distribution.bbb3d.renderfarming.net/video/mp4/bbb_sunflower_1080p_30fps_normal.mp4']
        });
        clapper.run();
    });

    box.append(label);
    box.append(button);
    grid.attach(box, 0, 0, 1, 1);

    window.set_child(grid);
    window.present();
});
app.run([]);
