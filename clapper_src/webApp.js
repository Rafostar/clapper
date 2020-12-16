const { Gio, GObject } = imports.gi;
const Debug = imports.clapper_src.debug;
const Misc = imports.clapper_src.misc;

let { debug } = Debug;

var WebApp = GObject.registerClass(
class ClapperWebApp extends Gio.SubprocessLauncher
{
    _init()
    {
        const flags = Gio.SubprocessFlags.STDOUT_SILENCE
            | Gio.SubprocessFlags.STDERR_SILENCE;

        super._init({ flags });

        this.daemonApp = null;
        this.isDaemonClosing = false;
    }

    startDaemonApp()
    {
        if(this.daemonApp)
            return;

        this.daemonApp = this.spawnv([Misc.appId + '.Daemon']);
        this.daemonApp.wait_async(null, this._onDaemonClosed.bind(this));

        debug('daemon app started');
    }

    stopDaemonApp()
    {
        if(!this.daemonApp || this.isDaemonClosing)
            return;

        this.isDaemonClosing = true;
        this.daemonApp.force_exit();

        debug('send stop signal to daemon app');
    }

    _onDaemonClosed(proc, result)
    {
        let hadError;

        try {
            hadError = proc.wait_finish(result);
        }
        catch(err) {
            debug(err);
        }

        this.daemonApp = null;
        this.isDaemonClosing = false;

        if(hadError)
            debug('daemon app exited with error or was forced to close');

        debug('daemon app closed');
    }
});
