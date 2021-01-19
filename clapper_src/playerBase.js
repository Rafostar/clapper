const { Gio, GLib, GObject, Gst, GstPlayer, Gtk } = imports.gi;
const Debug = imports.clapper_src.debug;
const Misc = imports.clapper_src.misc;
const { PlaylistWidget } = imports.clapper_src.playlist;
const { WebApp } = imports.clapper_src.webApp;

const { debug } = Debug;
const { settings } = Misc;

let WebServer;

var PlayerBase = GObject.registerClass(
class ClapperPlayerBase extends GstPlayer.Player
{
    _init()
    {
        if(!Gst.is_initialized())
            Gst.init(null);

        const plugin = 'gtk4glsink';
        const gtk4glsink = Gst.ElementFactory.make(plugin, null);

        if(!gtk4glsink) {
            debug(new Error(
                `Could not load "${plugin}".`
                    + ' Do you have gstreamer-plugins-good-gtk4 installed?'
            ));
        }

        const glsinkbin = Gst.ElementFactory.make('glsinkbin', null);
        glsinkbin.sink = gtk4glsink;

        const context = GLib.MainContext.ref_thread_default();
        const acquired = context.acquire();
        debug(`default context acquired: ${acquired}`);

        const dispatcher = new GstPlayer.PlayerGMainContextSignalDispatcher({
            application_context: context,
        });
        const renderer = new GstPlayer.PlayerVideoOverlayVideoRenderer({
            video_sink: glsinkbin
        });

        super._init({
            signal_dispatcher: dispatcher,
            video_renderer: renderer
        });

        this.widget = gtk4glsink.widget;
        this.widget.vexpand = true;
        this.widget.hexpand = true;

        this.state = GstPlayer.PlayerState.STOPPED;
        this.visualization_enabled = false;

        this.webserver = null;
        this.webapp = null;
        this.playlistWidget = new PlaylistWidget();

        this.set_all_plugins_ranks();
        this.set_initial_config();
        this.set_and_bind_settings();

        settings.connect('changed', this._onSettingsKeyChanged.bind(this));

        /* FIXME: additional reference for working around GstPlayer
         * buggy signal dispatcher on self. Remove when ported to BUS API */
        this.ref();
    }

    set_and_bind_settings()
    {
        const settingsToSet = [
            'seeking-mode',
            'audio-offset',
            'subtitle-offset',
            'play-flags',
            'webserver-enabled'
        ];

        for(let key of settingsToSet)
            this._onSettingsKeyChanged(settings, key);

        const flag = Gio.SettingsBindFlags.GET;
        settings.bind('subtitle-font', this.pipeline, 'subtitle_font_desc', flag);
    }

    set_initial_config()
    {
        const gstPlayerConfig = {
            position_update_interval: 1000,
            user_agent: 'clapper',
        };

        for(let option of Object.keys(gstPlayerConfig))
            this.set_config_option(option, gstPlayerConfig[option]);

        this.set_mute(false);

        /* FIXME: change into option in preferences */
        const pipeline = this.get_pipeline();
        pipeline.ring_buffer_max_size = 8 * 1024 * 1024;
    }

    set_config_option(option, value)
    {
        const setOption = GstPlayer.Player[`config_set_${option}`];
        if(!setOption)
            return debug(`unsupported option: ${option}`, 'LEVEL_WARNING');

        const config = this.get_config();
        setOption(config, value);
        const success = this.set_config(config);

        if(!success)
            debug(`could not change option: ${option}`);
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
        const gstRegistry = Gst.Registry.get();
        const feature = gstRegistry.lookup_feature(name);
        if(!feature)
            return debug(`plugin unavailable: ${name}`);

        const oldRank = feature.get_rank();
        if(rank === oldRank)
            return;

        feature.set_rank(rank);
        debug(`changed rank: ${oldRank} -> ${rank} for ${name}`);
    }

    draw_black(isEnabled)
    {
        this.widget.ignore_textures = isEnabled;

        if(this.state !== GstPlayer.PlayerState.PLAYING)
            this.widget.queue_render();
    }

    emitWs(action, value)
    {
        if(!this.webserver)
            return;

        this.webserver.sendMessage({ action, value });
    }

    receiveWs(action, value)
    {
        debug(`unhandled WebSocket action: ${action}`);
    }

    _onSettingsKeyChanged(settings, key)
    {
        let root, value, action;

        switch(key) {
            case 'seeking-mode':
                const isSeekMode = (typeof this.set_seek_mode !== 'undefined');
                this.seekingMode = settings.get_string('seeking-mode');
                switch(this.seekingMode) {
                    case 'fast':
                        if(isSeekMode)
                            this.set_seek_mode(GstPlayer.PlayerSeekMode.FAST);
                        else
                            this.set_config_option('seek_fast', true);
                        break;
                    case 'accurate':
                        if(isSeekMode)
                            this.set_seek_mode(GstPlayer.PlayerSeekMode.ACCURATE);
                        else {
                            this.set_config_option('seek_fast', false);
                            this.set_config_option('seek_accurate', true);
                        }
                        break;
                    default:
                        if(isSeekMode)
                            this.set_seek_mode(GstPlayer.PlayerSeekMode.DEFAULT);
                        else {
                            this.set_config_option('seek_fast', false);
                            this.set_config_option('seek_accurate', false);
                        }
                        break;
                }
                break;
            case 'render-shadows':
                root = this.widget.get_root();
                /* Editing theme of someone else app is taboo */
                if(!root || !root.isClapperApp)
                    break;

                const gpuClass = 'gpufriendly';
                const renderShadows = settings.get_boolean(key);
                const hasShadows = !root.has_css_class(gpuClass);

                if(renderShadows === hasShadows)
                    break;

                action = (renderShadows) ? 'remove' : 'add';
                root[action + '_css_class'](gpuClass);
                break;
            case 'audio-offset':
                value = Math.round(settings.get_double(key) * -1000000);
                this.set_audio_video_offset(value);
                debug(`set audio-video offset: ${value}`);
                break;
            case 'subtitle-offset':
                value = Math.round(settings.get_double(key) * -1000000);
                this.set_subtitle_video_offset(value);
                debug(`set subtitle-video offset: ${value}`);
                break;
            case 'dark-theme':
            case 'brighter-sliders':
                root = this.widget.get_root();
                if(!root || !root.isClapperApp)
                    break;

                const brightClass = 'brightscale';
                const isBrighter = root.has_css_class(brightClass);

                if(key === 'dark-theme' && isBrighter && !settings.get_boolean(key)) {
                    root.remove_css_class(brightClass);
                    debug('remove brighter sliders');
                    break;
                }

                const setBrighter = settings.get_boolean('brighter-sliders');
                if(setBrighter === isBrighter)
                    break;

                action = (setBrighter) ? 'add' : 'remove';
                root[action + '_css_class'](brightClass);
                debug(`${action} brighter sliders`);
                break;
            case 'play-flags':
                const initialFlags = this.pipeline.flags;
                const settingsFlags = settings.get_int(key);

                if(initialFlags === settingsFlags)
                    break;

                this.pipeline.flags = settingsFlags;
                debug(`changed play flags: ${initialFlags} -> ${settingsFlags}`);
                break;
            case 'webserver-enabled':
            case 'webapp-enabled':
                const webserverEnabled = settings.get_boolean('webserver-enabled');

                if(webserverEnabled) {
                    if(!WebServer) {
                        /* Probably most users will not use this,
                         * so conditional import for faster startup */
                        WebServer = imports.clapper_src.webServer.WebServer;
                    }

                    if(!this.webserver) {
                        this.webserver = new WebServer(settings.get_int('webserver-port'));
                        this.webserver.passMsgData = this.receiveWs.bind(this);
                    }
                    this.webserver.startListening();

                    const webappEnabled = settings.get_boolean('webapp-enabled');

                    if(!this.webapp && !webappEnabled)
                        break;

                    if(webappEnabled) {
                        if(!this.webapp)
                            this.webapp = new WebApp();

                        this.webapp.startDaemonApp(settings.get_int('webapp-port'));
                    }
                }
                else if(this.webserver) {
                    /* remote app will close when connection is lost
                     * which will cause the daemon to close too */
                    this.webserver.stopListening();
                }
                break;
            case 'webserver-port':
                if(!this.webserver)
                    break;

                this.webserver.setListeningPort(settings.get_int(key));
                break;
            default:
                break;
        }
    }
});
