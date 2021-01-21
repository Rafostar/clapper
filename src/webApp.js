const { Gio, GObject } = imports.gi;
const Debug = imports.src.debug;
const Misc = imports.src.misc;

const { debug } = Debug;

var WebApp = GObject.registerClass(
class ClapperWebApp extends Gio.SubprocessLauncher
{
    _init()
    {
        const flags = Gio.SubprocessFlags.STDOUT_SILENCE
            | Gio.SubprocessFlags.STDERR_SILENCE;

        super._init({ flags });

        this.daemonApp = null;
    }

    startDaemonApp(port)
    {
        if(this.daemonApp)
            return;

        this.daemonApp = this.spawnv([Misc.appId + '.Daemon', String(port)]);
        this.daemonApp.wait_async(null, this._onDaemonClosed.bind(this));

        debug('daemon app started');
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

        if(hadError)
            debug('daemon app exited with error or was forced to close');

        debug('daemon app closed');
    }
});
