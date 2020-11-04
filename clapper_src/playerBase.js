const { Gio, GLib, GObject, Gst, GstPlayer, Gtk } = imports.gi;
const Debug = imports.clapper_src.debug;
const Shared = imports.clapper_src.shared;

/* PlayFlags are not exported through GI */
Gst.PlayFlags = {
  VIDEO: 1,
  AUDIO: 2,
  TEXT: 4,
  VIS: 8,
  SOFT_VOLUME: 16,
  NATIVE_AUDIO: 32,
  NATIVE_VIDEO: 64,
  DOWNLOAD: 128,
  BUFFERING: 256,
  DEINTERLACE: 512,
  SOFT_COLORBALANCE: 1024,
  FORCE_FILTERS: 2048,
  FORCE_SW_DECODERS: 4096,
};

let { debug } = Debug;
let { settings } = Shared;

var PlayerBase = GObject.registerClass(
class ClapperPlayerBase extends GstPlayer.Player
{
    _init()
    {
        if(!Gst.is_initialized())
            Gst.init(null);

        let plugin = 'gtk4glsink';
        let gtkglsink = Gst.ElementFactory.make(plugin, null);

        if(!gtkglsink) {
            return debug(new Error(
                `Could not load "${plugin}".`
                    + ' Do you have gstreamer-plugins-good-gtk4 installed?'
            ));
        }

        let glsinkbin = Gst.ElementFactory.make('glsinkbin', null);
        glsinkbin.sink = gtkglsink;

        let context = GLib.MainContext.ref_thread_default();
        let acquired = context.acquire();
        debug(`default context acquired: ${acquired}`);

        let dispatcher = new GstPlayer.PlayerGMainContextSignalDispatcher({
            application_context: context,
        });
        let renderer = new GstPlayer.PlayerVideoOverlayVideoRenderer({
            video_sink: glsinkbin
        });

        super._init({
            signal_dispatcher: dispatcher,
            video_renderer: renderer
        });

        this.widget = gtkglsink.widget;
        this.widget.vexpand = true;
        this.widget.hexpand = true;
        this.widget.set_opacity(0);

        this.visualization_enabled = false;

        this.set_all_plugins_ranks();
        this.set_initial_config();
        this.set_and_bind_settings();

        settings.connect('changed', this._onSettingsKeyChanged.bind(this));
    }

    set_and_bind_settings()
    {
        let settingsToSet = [
            'seeking-mode',
        ];

        for(let key of settingsToSet)
            this._onSettingsKeyChanged(settings, key);

        //let flag = Gio.SettingsBindFlags.GET;
    }

    set_initial_config()
    {
        let gstPlayerConfig = {
            position_update_interval: 1000,
            user_agent: 'clapper',
        };

        for(let option of Object.keys(gstPlayerConfig))
            this.set_config_option(option, gstPlayerConfig[option]);

        this.set_mute(false);
    }

    set_config_option(option, value)
    {
        let setOption = GstPlayer.Player[`config_set_${option}`];
        if(!setOption)
            return debug(`unsupported option: ${option}`, 'LEVEL_WARNING');

        let config = this.get_config();
        setOption(config, value);
        this.set_config(config);
    }

    /* FIXME: add in prefs and move to bind_settings() */
    set_subtitle_font_desc(desc)
    {
        let pipeline = this.get_pipeline();
        pipeline.subtitle_font_desc = desc;
    }

    set_all_plugins_ranks()
    {
        let data = [];

        /* Set empty plugin list if someone messed it externally */
        try {
            data = JSON.parse(settings.get_string('plugin-ranking'));
            if(!Array.isArray(data))
                throw new Error('plugin ranking data is not an array!');
        }
        catch(err) {
            debug(err);
            settings.set_string('plugin-ranking', "[]");
        }

        for(let plugin of data) {
            if(!plugin.apply || !plugin.name)
                continue;

            this.set_plugin_rank(plugin.name, plugin.rank);
        }
    }

    set_plugin_rank(name, rank)
    {
        let gstRegistry = Gst.Registry.get();
        let feature = gstRegistry.lookup_feature(name);
        if(!feature)
            return debug(`plugin unavailable: ${name}`);

        let oldRank = feature.get_rank();
        if(rank === oldRank)
            return;

        feature.set_rank(rank);
        debug(`changed rank: ${oldRank} -> ${rank} for ${name}`);
    }

    _onSettingsKeyChanged(settings, key)
    {
        switch(key) {
            case 'seeking-mode':
                this.seekingMode = settings.get_string('seeking-mode');
                switch(this.seekingMode) {
                    case 'accurate':
                        this.set_config_option('seek_accurate', true);
                        break;
                    default:
                        this.set_config_option('seek_accurate', false);
                        break;
                }
                break;
            default:
                break;
        }
    }
});
