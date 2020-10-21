const { GObject, Gst, Gtk } = imports.gi;
const Misc = imports.clapper_src.misc;

var actions = [
    about
];

var accels = [
    ['app.quit', ['q']],
];

function about(window, appName)
{
    let gstVer = [
        Gst.VERSION_MAJOR, Gst.VERSION_MINOR, Gst.VERSION_MICRO
    ].join('.');

    let gtkVer = [
        Gtk.MAJOR_VERSION, Gtk.MINOR_VERSION, Gtk.MICRO_VERSION
    ].join('.');

    let osInfo = [
        'GTK version' + ': ' + gtkVer,
        'GStreamer version' + ': ' + gstVer
    ].join('\n');

    let aboutDialog = new Gtk.AboutDialog({
        program_name: appName,
        comments: 'A GNOME media player powered by GStreamer',
        version: Misc.getClapperVersion(),
        authors: ['Rafał Dzięgiel'],
        artists: ['Rafał Dzięgiel'],
        license_type: Gtk.License.GPL_3_0,
        logo_icon_name: 'com.github.rafostar.Clapper',
        website: 'https://github.com/Rafostar/clapper',
        modal: true,
        system_information: osInfo,
        transient_for: window
    });

    aboutDialog.present();
}
