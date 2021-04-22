const Dialogs = imports.src.dialogs;

var actions = {
    open_local: {
        run: (window) => new Dialogs.FileChooser(window),
        accels: ['<Ctrl>O'],
    },
    open_uri: {
        run: (window) => new Dialogs.UriDialog(window),
        accels: ['<Ctrl>U'],
    },
    prefs: {
        run: (window) => new Dialogs.PrefsDialog(window),
    },
    about: {
        run: (window) => new Dialogs.AboutDialog(window),
    }
};
