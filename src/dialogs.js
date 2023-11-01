const { Adw, Gdk, Gio, GObject, Gst, Gtk } = imports.gi;
const System = imports.system;
const Debug = imports.src.debug;
const FileOps = imports.src.fileOps;
const Misc = imports.src.misc;

const { debug } = Debug;

var UriDialog = GObject.registerClass({
    GTypeName: 'ClapperUriDialog',
},
class ClapperUriDialog extends Gtk.Dialog
{
    _init(window)
    {
        super._init({
            transient_for: window,
            modal: true,
            use_header_bar: true,
            title: _('Open URI'),
            default_width: 420,
        });

        const contentBox = this.get_content_area();
        contentBox.vexpand = true;
        contentBox.add_css_class('uridialogbox');

        const linkEntry = new Gtk.Entry({
            activates_default: true,
            truncate_multiline: true,
            height_request: 38,
            hexpand: true,
            valign: Gtk.Align.CENTER,
            input_purpose: Gtk.InputPurpose.URL,
            placeholder_text: _('Enter or drop URI here'),
        });
        linkEntry.connect('notify::text', this._onTextNotify.bind(this));
        contentBox.append(linkEntry);

        this.add_button(_('Cancel'), Gtk.ResponseType.CANCEL);
        this.add_button(_('Open'), Gtk.ResponseType.OK);

        this.set_default_response(Gtk.ResponseType.OK);
        this.set_response_sensitive(Gtk.ResponseType.OK, false);

        const display = Gdk.Display.get_default();
        const clipboard = (display) ? display.get_clipboard() : null;

        if(clipboard)
            clipboard.read_text_async(null, this._readTextAsyncCb.bind(this));

        this.present();
    }

    vfunc_response(respId)
    {
        if(respId === Gtk.ResponseType.OK) {
            const contentBox = this.get_content_area();
            const linkEntry = contentBox.get_last_child();
            const { player } = this.transient_for.child;

            player.set_playlist([linkEntry.text]);
        }

        this.destroy();
    }

    _onTextNotify(entry)
    {
        const isUriValid = (entry.text.length)
            ? Gst.uri_is_valid(entry.text)
            : false;

        this.set_response_sensitive(Gtk.ResponseType.OK, isUriValid);
    }

    _readTextAsyncCb(clipboard, result)
    {
        let uri = null;

        try {
            uri = clipboard.read_text_finish(result);
        }
        catch(err) {
            debug(`could not read clipboard: ${err.message}`);
        }

        if(!uri || !Gst.uri_is_valid(uri))
            return;

        const contentBox = this.get_content_area();
        const linkEntry = contentBox.get_last_child();

        linkEntry.set_text(uri);
        linkEntry.select_region(0, -1);
    }
});

var ResumeDialog = GObject.registerClass({
    GTypeName: 'ClapperResumeDialog',
},
class ClapperResumeDialog extends Adw.MessageDialog
{
    _init(window, resumeInfo)
    {
        const percentage = Math.round((resumeInfo.time / resumeInfo.duration) * 100);

        const msg = [
            '<b>' + _('Title') + `:</b> ${resumeInfo.title}`,
            '<b>' + _('Completed') + `:</b> ${percentage}%`
        ].join('\n');

        super._init({
            transient_for: window,
            modal: true,
            heading: _('Resume Playback?'),
            body_use_markup: true,
            body: msg,
        });

        this.add_response('cancel', _('Cancel'));
        this.add_response('resume', _('Resume'));
        this.set_close_response('cancel');
        this.set_default_response('resume');
        this.set_response_appearance('resume', Adw.ResponseAppearance.SUGGESTED);

        this.resumeInfo = resumeInfo;

        this.present();
    }

    vfunc_response(respId)
    {
        const { player } = this.transient_for.child;

        if(respId === 'resume')
            player.seek_seconds(this.resumeInfo.time);

        this.destroy();
    }
});

function showOpenLocalDialog(window)
{
    const filters = new Gio.ListStore();
    const filter = new Gtk.FileFilter({
        name: 'Media Files',
    });
    filter.add_mime_type('video/*');
    filter.add_mime_type('audio/*');
    filter.add_mime_type('application/claps');
    Misc.subsMimes.forEach(mime => filter.add_mime_type(mime));
    filters.append(filter);

    const fileDialog = new Gtk.FileDialog({modal: true});
    fileDialog.set_filters(filters);
    fileDialog.open_multiple(window, null, _handleOpenLocal.bind(window.application));
}

function _handleOpenLocal(fileDialog, res)
{
    try {
        const files = fileDialog.open_multiple_finish(res);
        const filesArray = [];

        let index = 0;
        let file;

        while((file = files.get_item(index))) {
            filesArray.push(file);
            index++;
        }

        const isHandlesOpen = Boolean(
            this.flags & Gio.ApplicationFlags.HANDLES_OPEN
        );

        /* Remote app does not handle open */
        if(isHandlesOpen)
            this.open(filesArray, "");
        else
            this._openFilesAsync(filesArray);
    }
    catch(e) {
        if(!e.matches(Gtk.DialogError.quark(), Gtk.DialogError.DISMISSED))
            throw e;
    }
}

function showExportPlaylistDialog(window)
{
    const filters = new Gio.ListStore();
    const filter = new Gtk.FileFilter({
        name: 'Playlist Files',
    });
    filter.add_mime_type('application/claps');
    filters.append(filter);

    const fileDialog = new Gtk.FileDialog({modal: true});
    fileDialog.set_filters(filters);
    fileDialog.set_initial_name('playlist.claps');
    fileDialog.save(window, null, _handleExportPlaylist.bind(window.child.player.playlistWidget));
}

function _handleExportPlaylist(fileDialog, res)
{
    try {
        const file = fileDialog.save_finish(res);
        const playlist = this.getPlaylist(true);

        FileOps.saveFileSimplePromise(file, playlist.join('\n'))
            .then(() => {
                debug(`exported playlist to file: ${file.get_path()}`);
            })
            .catch(err => {
                debug(err);
            });
    }
    catch(e) {
        if(!e.matches(Gtk.DialogError.quark(), Gtk.DialogError.DISMISSED))
            throw e;
    }
}

function showAboutDialog(window)
{
    const gstVer = [
        Gst.VERSION_MAJOR, Gst.VERSION_MINOR, Gst.VERSION_MICRO
    ].join('.');

    const gtkVer = [
        Gtk.MAJOR_VERSION, Gtk.MINOR_VERSION, Gtk.MICRO_VERSION
    ].join('.');

    const adwVer = [
        Adw.MAJOR_VERSION, Adw.MINOR_VERSION, Adw.MICRO_VERSION
    ].join('.');

    const gjsVerStr = String(System.version);
    let gjsVer = '';

    gjsVer += gjsVerStr.charAt(0) + '.';
    gjsVer += gjsVerStr.charAt(1) + gjsVerStr.charAt(2) + '.';
    if(gjsVerStr.charAt(3) !== '0')
        gjsVer += gjsVerStr.charAt(3);
    gjsVer += gjsVerStr.charAt(4);

    const osInfo = [
        _('GTK version: %s').format(gtkVer),
        _('Adwaita version: %s').format(adwVer),
        _('GStreamer version: %s').format(gstVer),
        _('GJS version: %s').format(gjsVer)
    ].join('\n');

    const aboutWindow = new Adw.AboutWindow({
        transient_for: window,
        application_name: Misc.appName,
        version: pkg.version,
        developer_name: 'Rafał Dzięgiel',
        developers: ['Rafał Dzięgiel'],
        artists: ['Rafał Dzięgiel'],
        /* TRANSLATORS: Put your name(s) here for credits or leave untranslated */
        translator_credits: _('translator-credits'),
        license_type: Gtk.License.GPL_3_0,
        application_icon: 'com.github.rafostar.Clapper',
        website: 'https://rafostar.github.io/clapper',
        issue_url: 'https://github.com/Rafostar/clapper/issues/new',
        debug_info: osInfo,
    });

    aboutWindow.present();
}
