const { GObject, Gtk } = imports.gi;
const Dialogs = imports.src.dialogs;

var actions = {
    openLocal: {
        run: (window) => new Dialogs.FileChooser(window),
        accels: ['<Ctrl>O'],
    },
    openUri: {
        run: (window) => new Dialogs.UriDialog(window),
        accels: ['<Ctrl>U'],
    },
    prefs: {
        run: (window) => new Dialogs.PrefsDialog(window),
    },
    about: {
        run: (window) => new Dialogs.AboutDialog(window),
    },
};
