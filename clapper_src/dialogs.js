const { GObject, Gtk, Gst } = imports.gi;
const Debug = imports.clapper_src.debug;
const Misc = imports.clapper_src.misc;
const Prefs = imports.clapper_src.prefs;
const PrefsBase = imports.clapper_src.prefsBase;

let { debug } = Debug;

var FileChooser = GObject.registerClass(
class ClapperFileChooser extends Gtk.FileChooserNative
{
    _init(window)
    {
        super._init({
            transient_for: window,
            modal: true,
        });

        let filter = new Gtk.FileFilter({
            name: 'Media Files',
        });
        filter.add_mime_type('video/*');
        filter.add_mime_type('audio/*');
        filter.add_mime_type('application/claps');
        this.add_filter(filter);

        this.responseSignal = this.connect('response', this._onResponse.bind(this));

        /* File chooser closes itself when nobody is holding its ref */
        this.ref();
        this.show();
    }

    _onResponse(filechooser, response)
    {
        debug('closing file chooser dialog');

        this.disconnect(this.responseSignal);
        this.responseSignal = null;

        if(response === Gtk.ResponseType.ACCEPT) {
            let file = this.get_file();
            let { player } = this.get_transient_for().get_child();

            player.set_media(file.get_uri());
        }

        this.unref();
    }
});

var UriDialog = GObject.registerClass(
class ClapperUriDialog extends Gtk.Dialog
{
    _init(window)
    {
        super._init({
            transient_for: window,
            destroy_with_parent: true,
            modal: true,
            title: 'Open URI',
            default_width: 460,
        });

        let box = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
            valign: Gtk.Align.CENTER,
            spacing: 6,
        });
        box.add_css_class('uridialogbox');

        let linkEntry = new Gtk.Entry({
            activates_default: true,
            truncate_multiline: true,
            width_request: 220,
            height_request: 36,
            hexpand: true,
        });
        linkEntry.set_placeholder_text("Enter or drop URI here");
        linkEntry.connect('notify::text', this._onTextNotify.bind(this));
        box.append(linkEntry);

        let openButton = new Gtk.Button({
            label: "Open",
            halign: Gtk.Align.END,
            sensitive: false,
        });
        openButton.connect('clicked', this._onOpenButtonClicked.bind(this));
        box.append(openButton);

        let area = this.get_content_area();
        area.append(box);

        this.closeSignal = this.connect('close-request', this._onCloseRequest.bind(this));

        this.ref();
        this.show();
    }

    openUri(uri)
    {
        let { player } = this.get_transient_for().get_child();
        player.set_media(uri);

        this.close();
    }

    _onTextNotify(entry)
    {
        let isUriValid = (entry.text.length)
            ? Gst.uri_is_valid(entry.text)
            : false;

        let button = entry.get_next_sibling();
        button.set_sensitive(isUriValid);
    }

    _onOpenButtonClicked(button)
    {
        let entry = button.get_prev_sibling();
        this.openUri(entry.text);
    }

    _onCloseRequest(dialog)
    {
        debug('closing URI dialog');

        dialog.disconnect(this.closeSignal);
        this.closeSignal = null;
    }
});

var PrefsDialog = GObject.registerClass(
class ClapperPrefsDialog extends Gtk.Dialog
{
    _init(window)
    {
        super._init({
            transient_for: window,
            destroy_with_parent: true,
            modal: true,
            title: 'Preferences',
            default_width: 460,
            default_height: 400,
        });

        let pages = [
            {
                title: 'Player',
                pages: [
                    {
                        title: 'General',
                        widget: Prefs.GeneralPage,
                    },
                    {
                        title: 'Behaviour',
                        widget: Prefs.BehaviourPage,
                    }
                ]
            },
            {
                title: 'Advanced',
                pages: [
                    {
                        title: 'GStreamer',
                        widget: Prefs.GStreamerPage,
                    },
                    {
                        title: 'Tweaks',
                        widget: Prefs.TweaksPage,
                    }
                ]
            }
        ];

        let prefsNotebook = new PrefsBase.Notebook(pages);
        prefsNotebook.add_css_class('prefsnotebook');

        let area = this.get_content_area();
        area.append(prefsNotebook);

        this.closeSignal = this.connect('close-request', this._onCloseRequest.bind(this));

        this.ref();
        this.show();
    }

    _onCloseRequest(dialog)
    {
        debug('closing prefs dialog');

        dialog.disconnect(this.closeSignal);
        this.closeSignal = null;

        let area = dialog.get_content_area();
        let notebook = area.get_first_child();
        notebook._onClose();
    }
});

var AboutDialog = GObject.registerClass(
class ClapperAboutDialog extends Gtk.AboutDialog
{
    _init(window)
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

        super._init({
            transient_for: window,
            destroy_with_parent: true,
            modal: true,
            program_name: Misc.appName,
            comments: 'A GNOME media player powered by GStreamer',
            version: Misc.getClapperVersion(),
            authors: ['Rafał Dzięgiel'],
            artists: ['Rafał Dzięgiel'],
            license_type: Gtk.License.GPL_3_0,
            logo_icon_name: 'com.github.rafostar.Clapper',
            website: 'https://github.com/Rafostar/clapper',
            system_information: osInfo,
        });

        this.closeSignal = this.connect('close-request', this._onCloseRequest.bind(this));

        this.ref();
        this.show();
    }

    _onCloseRequest(dialog)
    {
        debug('closing about dialog');

        dialog.disconnect(this.closeSignal);
        this.closeSignal = null;
    }
});
