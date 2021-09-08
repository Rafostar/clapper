const { Gio, GLib, GObject } = imports.gi;
const Debug = imports.debug;

const { debug } = Debug;

var Daemon = GObject.registerClass({
    GTypeName: 'ClapperDaemon',
},
class ClapperDaemon extends Gio.SubprocessLauncher
{
    _init()
    {
        const port = ARGV[0] || 8080;

        /* FIXME: show output when debugging is on */
        const flags = Gio.SubprocessFlags.STDOUT_SILENCE
            | Gio.SubprocessFlags.STDERR_SILENCE;

        super._init({ flags });

        this.errMsg = 'exited with error or was forced to close';
        this.loop = GLib.MainLoop.new(null, false);

        this.broadwayd = this.spawnv(['gtk4-broadwayd', '--port=' + port]);
        this.broadwayd.wait_async(null, this._onBroadwaydClosed.bind(this));

        this.setenv('GDK_BACKEND', 'broadway', true);

        const remoteApp = this.spawnv(['com.github.rafostar.Clapper.Remote']);
        remoteApp.wait_async(null, this._onRemoteClosed.bind(this));

        this.loop.run();
    }

    _checkProcResult(proc, result)
    {
        let hadError = false;

        try {
            hadError = proc.wait_finish(result);
        }
        catch(err) {
            debug(err);
        }

        return hadError;
    }

    _onBroadwaydClosed(proc, result)
    {
        const hadError = this._checkProcResult(proc, result);

        if(hadError)
            debug(`broadwayd ${this.errMsg}`);

        this.broadwayd = null;
        this.loop.quit();
    }

    _onRemoteClosed(proc, result)
    {
        const hadError = this._checkProcResult(proc, result);

        if(hadError)
            debug(`remote app ${this.errMsg}`);

        if(this.broadwayd)
            this.broadwayd.force_exit();
    }
});
