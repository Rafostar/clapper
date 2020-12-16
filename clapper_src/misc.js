const { Gio, GstAudio, GstPlayer, Gdk, Gtk } = imports.gi;
const Debug = imports.clapper_src.debug;

var appName = 'Clapper';
var appId = 'com.github.rafostar.Clapper';

var clapperPath = null;
var clapperVersion = null;

var settings = new Gio.Settings({
    schema_id: appId,
});

var maxVolume = 1.5;
var isOldGtk = (Gtk.MINOR_VERSION === 99 && Gtk.MICRO_VERSION <= 4);

let { debug } = Debug;
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

function inhibitForState(state, window)
{
    let isInhibited = false;

    if(state === GstPlayer.PlayerState.PLAYING) {
        if(inhibitCookie)
            return;

        let app = window.get_application();
        let flags = Gtk.ApplicationInhibitFlags.SUSPEND
            | Gtk.ApplicationInhibitFlags.IDLE;

        inhibitCookie = app.inhibit(
            window,
            flags,
            'video is playing'
        );
        if(!inhibitCookie)
            debug(new Error('could not inhibit session!'));

        isInhibited = (inhibitCookie > 0);
    }
    else {
        //if(!inhibitCookie)
            return;

        /* Uninhibit seems to be broken as of GTK 3.99.4
        let app = window.get_application();
        app.uninhibit(inhibitCookie);
        inhibitCookie = null;
        */
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
    let minutes = ('0' + Math.floor(time / 60)).slice(-2);
    time -= minutes * 60;
    let seconds = ('0' + Math.floor(time)).slice(-2);

    let parsed = (hours) ? `${hours}:` : '';
    return parsed + `${minutes}:${seconds}`;
}

function getCubicValue(linearVal)
{
    return GstAudio.StreamVolume.convert_volume(
        GstAudio.StreamVolumeFormat.LINEAR,
        GstAudio.StreamVolumeFormat.CUBIC,
        linearVal
    );
}

function getLinearValue(cubicVal)
{
    return GstAudio.StreamVolume.convert_volume(
        GstAudio.StreamVolumeFormat.CUBIC,
        GstAudio.StreamVolumeFormat.LINEAR,
        cubicVal
    );
}
