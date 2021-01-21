const { GObject, Gst, Gtk } = imports.gi;
const Dialogs = imports.src.dialogs;

var actions = {
    openLocal: (window) => new Dialogs.FileChooser(window),
    openUri: (window) => new Dialogs.UriDialog(window),
    prefs: (window) => new Dialogs.PrefsDialog(window),
    about: (window) => new Dialogs.AboutDialog(window),
};

var accels = [
    ['app.quit', ['q']],
];
