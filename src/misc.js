const { Gio, GLib, Gdk, Gtk } = imports.gi;
const Debug = imports.src.debug;

const { debug } = Debug;

var appName = 'Clapper';
var appId = 'com.github.rafostar.Clapper';
var subsMimes = [
    'application/x-subrip',
    'text/x-ssa',
];
var timeColon = '∶';

var settings = new Gio.Settings({
    schema_id: appId,
});

var maxVolume = 1.5;

/* Keys must be lowercase */
const subsTitles = {
    sdh: 'SDH',
    cc: 'CC',
    traditional: 'Traditional',
    simplified: 'Simplified',
    honorifics: 'Honorifics',
};
const subsKeys = Object.keys(subsTitles);

let inhibitCookie;

function getResourceUri(path)
{
    const res = `file://${pkg.pkgdatadir}/${path}`;

    debug(`importing ${res}`);

    return res;
}

function getBuilderForName(name)
{
    return Gtk.Builder.new_from_file(`${pkg.pkgdatadir}/ui/${name}`);
}

function loadCustomCss()
{
    const cssProvider = new Gtk.CssProvider();

    cssProvider.load_from_path(`${pkg.pkgdatadir}/css/styles.css`);

    Gtk.StyleContext.add_provider_for_display(
        Gdk.Display.get_default(),
        cssProvider,
        Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}

function getClapperThemeIconUri()
{
    const display = Gdk.Display.get_default();
    if(!display) return null;

    const iconTheme = Gtk.IconTheme.get_for_display(display);
    if(!iconTheme || !iconTheme.has_icon(appId))
        return null;

    const iconPaintable = iconTheme.lookup_icon(appId, null, 256, 1,
        Gtk.TextDirection.NONE, Gtk.IconLookupFlags.FORCE_REGULAR
    );
    const iconFile = iconPaintable.get_file();
    if(!iconFile) return null;

    const iconPath = iconFile.get_path();
    if(!iconPath) return null;

    let substractName = iconPath.substring(
        iconPath.indexOf('/icons/') + 7, iconPath.indexOf('/scalable/')
    );
    if(!substractName || substractName.includes('/'))
        return null;

    substractName = substractName.toLowerCase();
    const postFix = (substractName === iconTheme.theme_name.toLowerCase())
        ? substractName
        : 'hicolor';
    const cacheIconName = `clapper-${postFix}.svg`;

    /* We need to have this icon placed in a folder
     * accessible from both app runtime and gnome-shell */
    const expectedFile = Gio.File.new_for_path(
        GLib.get_user_cache_dir() + `/${appId}/icons/${cacheIconName}`
    );
    if(!expectedFile.query_exists(null)) {
        debug('no cached icon file');

        const dirPath = expectedFile.get_parent().get_path();
        GLib.mkdir_with_parents(dirPath, 493); // octal 755
        iconFile.copy(expectedFile,
            Gio.FileCopyFlags.TARGET_DEFAULT_PERMS, null, null
        );
        debug(`icon copied to cache dir: ${cacheIconName}`);
    }
    const iconUri = expectedFile.get_uri();
    debug(`using cached clapper icon uri: ${iconUri}`);

    return iconUri;
}

function getSubsTitle(infoTitle)
{
    if(!infoTitle)
        return null;

    const searchName = infoTitle.toLowerCase();
    const found = subsKeys.find(key => key === searchName);

    return (found) ? subsTitles[found] : null;
}

function setAppInhibit(isInhibit, window)
{
    let isInhibited = false;

    if(isInhibit) {
        if(inhibitCookie)
            return;

        const app = window.get_application();

        inhibitCookie = app.inhibit(
            window,
            Gtk.ApplicationInhibitFlags.IDLE,
            'video is playing'
        );
        if(!inhibitCookie)
            debug(new Error('could not inhibit session!'));

        isInhibited = (inhibitCookie > 0);
    }
    else {
        if(!inhibitCookie)
            return;

        const app = window.get_application();
        app.uninhibit(inhibitCookie);
        inhibitCookie = null;
    }

    debug(`set prevent suspend to: ${isInhibited}`);
}

function getFormattedTime(time, showHours)
{
    let hours;

    if(showHours || time >= 3600) {
        hours = ('0' + Math.floor(time / 3600)).slice(-2);
        time -= hours * 3600;
    }
    const minutes = ('0' + Math.floor(time / 60)).slice(-2);
    time -= minutes * 60;
    const seconds = ('0' + Math.floor(time)).slice(-2);

    const parsed = (hours) ? `${hours}${timeColon}` : '';
    return parsed + `${minutes}${timeColon}${seconds}`;
}

function parsePlaylistFiles(filesArray)
{
    let index = filesArray.length;
    let subs = null;

    while(index--) {
        const file = filesArray[index];
        const filename = (file.get_basename)
            ? file.get_basename()
            : file.substring(file.lastIndexOf('/') + 1);

        const [type, isUncertain] = Gio.content_type_guess(filename, null);

        if(subsMimes.includes(type)) {
            subs = file;
            filesArray.splice(index, 1);
        }
    }

    /* We only support single video
     * with external subtitles */
    if(subs && filesArray.length > 1)
        subs = null;

    return [filesArray, subs];
}

function getFileFromLocalUri(uri)
{
    const file = Gio.file_new_for_uri(uri);

    if(!file.query_exists(null)) {
        debug(new Error(`file does not exist: ${file.get_path()}`));

        return null;
    }

    return file;
}

/* JS replacement of "Gst.Uri.get_protocol" */
function getUriProtocol(uri)
{
    const arr = uri.split(':');
    return (arr.length > 1) ? arr[0] : null;
}

function getIsTouch(gesture)
{
    const { source } = gesture.get_device();

    switch(source) {
        case Gdk.InputSource.PEN:
        case Gdk.InputSource.TOUCHSCREEN:
            return true;
        default:
            return false;
    }
}
