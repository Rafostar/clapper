const { Gio, GObject, Gtk, Gst } = imports.gi;
const System = imports.system;
const Debug = imports.src.debug;
const FileOps = imports.src.fileOps;
const Misc = imports.src.misc;

const { debug } = Debug;

var FileChooser = GObject.registerClass(
class ClapperFileChooser extends Gtk.FileChooserNative
{
    _init(window, purpose)
    {
        super._init({
            transient_for: window,
            modal: true,
        });

        switch(purpose) {
            case 'open_local':
                this._prepareOpenLocal();
                break;
            case 'export_playlist':
                this._prepareExportPlaylist();
                break;
            default:
                debug(new Error(`unknown file chooser purpose: ${purpose}`));
                break;
        }

        this.chooserPurpose = purpose;
        this.responseSignal = this.connect('response', this._onResponse.bind(this));

        /* File chooser closes itself when nobody is holding its ref */
        this.ref();
        this.show();
    }

    _prepareOpenLocal()
    {
        this.select_multiple = true;

        const filter = new Gtk.FileFilter({
            name: 'Media Files',
        });
        filter.add_mime_type('video/*');
        filter.add_mime_type('audio/*');
        filter.add_mime_type('application/claps');
        Misc.subsMimes.forEach(mime => filter.add_mime_type(mime));
        this.add_filter(filter);
    }

    _prepareExportPlaylist()
    {
        this.action = Gtk.FileChooserAction.SAVE;
        this.set_current_name('playlist.claps');

        const filter = new Gtk.FileFilter({
            name: 'Playlist Files',
        });
        filter.add_mime_type('application/claps');
        this.add_filter(filter);
    }

    _onResponse(filechooser, response)
    {
        debug('closing file chooser dialog');

        this.disconnect(this.responseSignal);
        this.responseSignal = null;

        if(response === Gtk.ResponseType.ACCEPT) {
            switch(this.chooserPurpose) {
                case 'open_local':
                    this._handleOpenLocal();
                    break;
                case 'export_playlist':
                    this._handleExportPlaylist();
                    break;
            }
        }

        this.unref();
        this.destroy();
    }

    _handleOpenLocal()
    {
        const files = this.get_files();
        const filesArray = [];

        let index = 0;
        let file;

        while((file = files.get_item(index))) {
            filesArray.push(file);
            index++;
        }

        const { application } = this.transient_for;
        const isHandlesOpen = Boolean(
            application.flags & Gio.ApplicationFlags.HANDLES_OPEN
        );

        /* Remote app does not handle open */
        if(isHandlesOpen)
           application.open(filesArray, "");
        else
           application._openFilesAsync(filesArray);
    }

    _handleExportPlaylist()
    {
        const file = this.get_file();
        const { playlistWidget } = this.transient_for.child.player;
        const playlist = playlistWidget.getPlaylist(true);

        FileOps.saveFileSimplePromise(file, playlist.join('\n'))
            .then(() => {
                debug(`exported playlist to file: ${file.get_path()}`);
            })
            .catch(err => {
                debug(err);
            });
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

        const box = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
            valign: Gtk.Align.CENTER,
            spacing: 6,
        });
        box.add_css_class('uridialogbox');

        const linkEntry = new Gtk.Entry({
            activates_default: true,
            truncate_multiline: true,
            width_request: 220,
            height_request: 36,
            hexpand: true,
        });
        linkEntry.set_placeholder_text("Enter or drop URI here");
        linkEntry.connect('notify::text', this._onTextNotify.bind(this));
        box.append(linkEntry);

        const openButton = new Gtk.Button({
            label: "Open",
            halign: Gtk.Align.END,
            sensitive: false,
        });
        openButton.connect('clicked', this._onOpenButtonClicked.bind(this));
        box.append(openButton);

        const area = this.get_content_area();
        area.append(box);

        this.closeSignal = this.connect('close-request', this._onCloseRequest.bind(this));
        this.show();
    }

    openUri(uri)
    {
        const { player } = this.get_transient_for().get_child();
        player.set_playlist([uri]);

        this.close();
    }

    _onTextNotify(entry)
    {
        const isUriValid = (entry.text.length)
            ? Gst.uri_is_valid(entry.text)
            : false;

        const button = entry.get_next_sibling();
        button.set_sensitive(isUriValid);
    }

    _onOpenButtonClicked(button)
    {
        const entry = button.get_prev_sibling();
        this.openUri(entry.text);
    }

    _onCloseRequest(dialog)
    {
        debug('closing URI dialog');

        dialog.disconnect(this.closeSignal);
        this.closeSignal = null;
    }
});

var ResumeDialog = GObject.registerClass(
class ClapperResumeDialog extends Gtk.MessageDialog
{
    _init(window, resumeInfo)
    {
        const percentage = Math.round((resumeInfo.time / resumeInfo.duration) * 100);

        const msg = [
            `<b>Title:</b> ${resumeInfo.title}`,
            `<b>Completed:</b> ${percentage}%`
        ].join('\n');

        super._init({
            transient_for: window,
            modal: true,
            message_type: Gtk.MessageType.QUESTION,
            buttons: Gtk.ButtonsType.YES_NO,
            text: 'Resume playback?',
            secondary_use_markup: true,
            secondary_text: msg,
        });

        this.resumeInfo = resumeInfo;

        this.set_default_response(Gtk.ResponseType.YES);
        this.connect('response', this._onResponse.bind(this));

        this.show();
    }

    _onResponse(dialog, respId)
    {
        const { player } = this.transient_for.child;

        if(respId === Gtk.ResponseType.YES)
            player.seek_seconds(this.resumeInfo.time);

        this.destroy();
    }
});

var AboutDialog = GObject.registerClass(
class ClapperAboutDialog extends Gtk.AboutDialog
{
    _init(window)
    {
        const gstVer = [
            Gst.VERSION_MAJOR, Gst.VERSION_MINOR, Gst.VERSION_MICRO
        ].join('.');

        const gtkVer = [
            Gtk.MAJOR_VERSION, Gtk.MINOR_VERSION, Gtk.MICRO_VERSION
        ].join('.');

        const gjsVerStr = String(System.version);
        let gjsVer = '';

        gjsVer += gjsVerStr.charAt(0) + '.';
        gjsVer += gjsVerStr.charAt(1) + gjsVerStr.charAt(2) + '.';
        if(gjsVerStr.charAt(3) !== '0')
            gjsVer += gjsVerStr.charAt(3);
        gjsVer += gjsVerStr.charAt(4);

        const osInfo = [
            'GTK ' + 'version' + ': ' + gtkVer,
            'GStreamer ' + 'version' + ': ' + gstVer,
            'GJS ' + 'version' + ': ' + gjsVer
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
            website: 'https://rafostar.github.io/clapper',
            system_information: osInfo,
        });

        this.closeSignal = this.connect('close-request', this._onCloseRequest.bind(this));
        this.show();
    }

    _onCloseRequest(dialog)
    {
        debug('closing about dialog');

        dialog.disconnect(this.closeSignal);
        this.closeSignal = null;
    }
});
