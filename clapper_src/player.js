const { GLib, GObject, Gst, GstPlayer } = imports.gi;
const Debug = imports.clapper_src.debug;

const GSTPLAYER_DEFAULTS = {
    position_update_interval: 1000,
    seek_accurate: false,
    user_agent: 'clapper',
};

let { debug } = Debug;

var Player = GObject.registerClass(
class ClapperPlayer extends GstPlayer.Player
{
    _init(opts)
    {
        opts = opts || {};
        Object.assign(opts, GSTPLAYER_DEFAULTS);

        let gtkglsink = Gst.ElementFactory.make('gtkglsink', null);
        let glsinkbin = Gst.ElementFactory.make('glsinkbin', null);
        glsinkbin.sink = gtkglsink;

        let dispatcher = new GstPlayer.PlayerGMainContextSignalDispatcher();
        let renderer = new GstPlayer.PlayerVideoOverlayVideoRenderer({
            video_sink: glsinkbin
        });

        super._init({
            signal_dispatcher: dispatcher,
            video_renderer: renderer
        });

        // assign elements to player for later access
        // and make sure that GJS will not free them early
        this.gtkglsink = gtkglsink;
        this.glsinkbin = glsinkbin;
        this.dispatcher = dispatcher;
        this.renderer = renderer;

        let config = this.get_config();

        for(let setting of Object.keys(GSTPLAYER_DEFAULTS)) {
            let setOption = GstPlayer.Player[`config_set_${setting}`];
            if(!setOption) {
                debug(`unsupported option: ${setting}`, 'LEVEL_WARNING');
                continue;
            }
            setOption(config, opts[setting]);
        }

        this.set_config(config);
        this.set_mute(false);

        this.loop = GLib.MainLoop.new(null, false);
        this.run_loop = opts.run_loop || false;
        this.widget = gtkglsink.widget;
        this.state = GstPlayer.PlayerState.STOPPED;

        this.connect('state_changed', this._onStateChanged.bind(this));
        this.connect('uri_loaded', this._onUriLoaded.bind(this));
        this.widget.connect('destroy', this._onWidgetDestroy.bind(this));
    }

    seek_seconds(position)
    {
        this.seek(position * 1000000000);
    }

    toggle_play()
    {
        let action = (this.state === GstPlayer.PlayerState.PLAYING)
            ? 'pause'
            : 'play';

        this[action]();
    }

    set_subtitle_font_desc(desc)
    {
        let pipeline = this.get_pipeline();
        pipeline.subtitle_font_desc = desc;
    }

    _onStateChanged(player, state)
    {
        this.state = state;

        if(
            this.run_loop
            && this.state === GstPlayer.PlayerState.STOPPED
            && this.loop.is_running()
        )
            this.loop.quit();
    }

    _onUriLoaded()
    {
        this.play();

        if(
            this.run_loop
            && !this.loop.is_running()
        )
            this.loop.run();
    }

    _onWidgetDestroy()
    {
        if(this.state !== GstPlayer.PlayerState.STOPPED)
            this.stop();
    }
});
