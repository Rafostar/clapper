const { Gio, GstAudio, GstPlayer, Gdk, Gtk } = imports.gi;
const Debug = imports.clapper_src.debug;

const { debug } = Debug;

var appName = 'Clapper';
var appId = 'com.github.rafostar.Clapper';

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

function inhibitForState(state, window)
{
    let isInhibited = false;

    if(state === GstPlayer.PlayerState.PLAYING) {
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
