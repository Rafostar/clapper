const { Gio, Gdk, Gtk } = imports.gi;
const Debug = imports.src.debug;

const { debug } = Debug;

var appName = 'Clapper';
var appId = 'com.github.rafostar.Clapper';
var subsMimes = [
    'application/x-subrip',
    'text/x-ssa',
];

var clapperPath = null;
var clapperVersion = null;

var settings = new Gio.Settings({
    schema_id: appId,
});

var maxVolume = 1.5;

let inhibitCookie;

function getClapperPath()
{
    return (clapperPath)
        ? clapperPath
        : (pkg)
        ? `${pkg.datadir}/${pkg.name}`
        : '.';
}

function getClapperVersion()
{
    return (clapperVersion)
        ? clapperVersion
        : (pkg)
        ? pkg.version
        : '';
}

function loadCustomCss()
{
    const clapperPath = getClapperPath();
    const cssProvider = new Gtk.CssProvider();

    cssProvider.load_from_path(`${clapperPath}/css/styles.css`);
    Gtk.StyleContext.add_provider_for_display(
        Gdk.Display.get_default(),
        cssProvider,
        Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
    );
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

    const parsed = (hours) ? `${hours}:` : '';
    return parsed + `${minutes}:${seconds}`;
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

function encodeHTML(text)
{
    return text.replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&apos;');
}

function decodeURIPlus(uri)
{
    return decodeURI(uri.replace(/\+/g, ' '));
}

function isHex(num)
{
    return Boolean(num.match(/[0-9a-f]+$/i));
}
