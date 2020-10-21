const { GstPlayer, Gtk } = imports.gi;
const Debug = imports.clapper_src.debug;

var clapperPath;
var clapperVersion;

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

function inhibitForState(state, window)
{
    let isInhibited = false;
    let flags = Gtk.ApplicationInhibitFlags.SUSPEND
        | Gtk.ApplicationInhibitFlags.IDLE;

    if(state === GstPlayer.PlayerState.PLAYING) {
        if(inhibitCookie)
            return;

        let app = window.get_application();

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

        /* Uninhibit seems to be broken as of GTK 3.99.2
        this.uninhibit(inhibitCookie);
        inhibitCookie = null;
        */
    }

    debug(`set prevent suspend to: ${isInhibited}`);
}

function getFormatedTime(time)
{
    let hours = ('0' + Math.floor(time / 3600)).slice(-2);
    time -= hours * 3600;
    let minutes = ('0' + Math.floor(time / 60)).slice(-2);
    time -= minutes * 60;
    let seconds = ('0' + Math.floor(time)).slice(-2);

    return `${hours}:${minutes}:${seconds}`;
}
