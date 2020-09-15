const { Gdk, GLib, GObject, Gtk, GstPlayer } = imports.gi;
const Debug = imports.clapper_src.debug;
const { Interface } = imports.clapper_src.interface;
const { Player } = imports.clapper_src.player;
const { Window } = imports.clapper_src.window;

const APP_NAME = pkg.name.substring(
    pkg.name.lastIndexOf('.') + 1
);

let { debug } = Debug;

var App = GObject.registerClass({
    Signals: {
        'ready': {
            param_types: [GObject.TYPE_BOOLEAN]
        },
    }
}, class ClapperApp extends Gtk.Application
{
    _init(opts)
    {
        GLib.set_prgname(APP_NAME);

        super._init({
            application_id: pkg.name
        });

        let defaults = {
            playlist: [],
        };
        Object.assign(this, defaults, opts);

        this.cssProvider = new Gtk.CssProvider();
        this.cssProvider.load_from_path(
            `${pkg.datadir}/${pkg.name}/css/styles.css`
        );

        this.window = null;
        this.interface = null;
        this.player = null;
        this.dragStartReady = false;

        this.connect('startup', this._buildUI.bind(this));
        this.connect('activate', this._openWindow.bind(this));
    }

    run(arr)
    {
        arr = arr || [];
        super.run(arr);
    }

    setHideCursorTimeout()
    {
        this.clearTimeout('hideCursor');
        this.hideCursorTimeout = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 1, () => {
            this.hideCursorTimeout = null;

            if(this.isCursorInPlayer)
                this.playerWindow.set_cursor(this.blankCursor);

            return GLib.SOURCE_REMOVE;
        });
    }

    setHideControlsTimeout()
    {
        this.clearTimeout('hideControls');
        this.hideControlsTimeout = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 3, () => {
            this.hideControlsTimeout = null;

            if(this.window.isFullscreen && this.isCursorInPlayer) {
                this.clearTimeout('updateTime');
                this.interface.revealControls(false);
            }

            return GLib.SOURCE_REMOVE;
        });
    }

    setUpdateTimeInterval()
    {
        this.clearTimeout('updateTime');
        let nextUpdate = this.interface.updateTime();
        this.updateTimeTimeout = GLib.timeout_add(GLib.PRIORITY_DEFAULT, nextUpdate, () => {
            this.updateTimeTimeout = null;

            if(this.window.isFullscreen)
                this.setUpdateTimeInterval();

            return GLib.SOURCE_REMOVE;
        });
    }

    clearTimeout(name)
    {
        if(!this[`${name}Timeout`])
            return;

        GLib.source_remove(this[`${name}Timeout`]);
        this[`${name}Timeout`] = null;

        if(name === 'updateTime')
            debug('cleared update time interval');
    }

    _buildUI()
    {
        this.window = new Window(this, APP_NAME);

        this.windowRealizeSignal = this.window.connect(
            'realize', this._onWindowRealize.bind(this)
        );
        this.window.connect(
            'key-press-event', this._onWindowKeyPressEvent.bind(this)
        );
        this.window.connect(
            'fullscreen-changed', this._onWindowFullscreenChanged.bind(this)
        );

        this.interface = new Interface();
        let headerBar = new Gtk.HeaderBar({
            title: APP_NAME,
            show_close_button: true,
        });
        headerBar.pack_end(this.interface.controls.openMenuButton);
        headerBar.pack_end(this.interface.controls.fullscreenButton);
        this.interface.addHeaderBar(headerBar, APP_NAME);
        this.interface.controls.fullscreenButton.connect(
            'clicked', () => this._onInterfaceToggleFullscreenClicked(true)
        );
        this.interface.controls.unfullscreenButton.connect(
            'clicked', () => this._onInterfaceToggleFullscreenClicked(false)
        );

        this.window.set_titlebar(this.interface.headerBar);
        this.window.add(this.interface);
    }

    _openWindow()
    {
        this.window.show_all();
    }

    _onWindowRealize()
    {
        this.window.disconnect(this.windowRealizeSignal);

        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(),
            this.cssProvider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        );

        this.player = new Player();
        this.player.widget.add_events(
            Gdk.EventMask.SCROLL_MASK
            | Gdk.EventMask.ENTER_NOTIFY_MASK
            | Gdk.EventMask.LEAVE_NOTIFY_MASK
        );
        this.interface.addPlayer(this.player);

        this.player.connect('warning', this._onPlayerWarning.bind(this));
        this.player.connect('error', this._onPlayerError.bind(this));
        this.player.connect('state-changed', this._onPlayerStateChanged.bind(this));

        this.interface.revealerTop.connect(
            'button-press-event', this._onPlayerButtonPressEvent.bind(this)
        );
        this.player.connectWidget(
            'button-press-event', this._onPlayerButtonPressEvent.bind(this)
        );
        this.player.connectWidget(
            'enter-notify-event', this._onPlayerEnterNotifyEvent.bind(this)
        );
        this.player.connectWidget(
            'leave-notify-event', this._onPlayerLeaveNotifyEvent.bind(this)
        );
        this.player.connectWidget(
            'motion-notify-event', this._onPlayerMotionNotifyEvent.bind(this)
        );

        /* Widget signals that are disconnected after first run */
        this._playerRealizeSignal = this.player.widget.connect(
            'realize', this._onPlayerRealize.bind(this)
        );
        this._playerDrawSignal = this.player.widget.connect(
            'draw', this._onPlayerDraw.bind(this)
        );

        this.player.widget.show();
    }

    _onWindowFullscreenChanged(window, isFullscreen)
    {
        // when changing fullscreen pango layout of popup is lost
        // and we need to re-add marks to the new layout
        this.interface.controls.setVolumeMarks(false);

        if(isFullscreen) {
            this.setUpdateTimeInterval();
            this.interface.showControls(true);
            this.setHideControlsTimeout();
            this.interface.controls.unfullscreenButton.set_sensitive(true);
            this.interface.controls.unfullscreenButton.show();
        }
        else {
            this.clearTimeout('updateTime');
            this.interface.showControls(false);
            this.interface.controls.unfullscreenButton.set_sensitive(false);
            this.interface.controls.unfullscreenButton.hide();
        }

        this.interface.setControlsOnVideo(isFullscreen);
        this.interface.controls.setVolumeMarks(true);
        this.interface.controls.setFullscreenMode(isFullscreen);
    }

    _onWindowKeyPressEvent(self, event)
    {
        let [res, key] = event.get_keyval();
        if(!res) return;

        //let keyName = Gdk.keyval_name(key);
        let bool = false;

        switch(key) {
            case Gdk.KEY_space:
            case Gdk.KEY_Return:
                this.player.toggle_play();
                break;
            case Gdk.KEY_Right:
                bool = true;
            case Gdk.KEY_Left:
                // disabled due to missing "seek on drop" support
                //this.interface.controls.handleScaleIncrement('position', bool);
                break;
            case Gdk.KEY_Up:
                bool = true;
            case Gdk.KEY_Down:
                this.interface.controls.handleScaleIncrement('volume', bool);
                break;
            case Gdk.KEY_F11:
                this.window.toggleFullscreen();
                break;
            case Gdk.KEY_Escape:
                if(this.window.isFullscreen)
                    this.window.unfullscreen();
                break;
            case Gdk.KEY_q:
            case Gdk.KEY_Q:
                this.window.destroy();
                break;
            default:
                break;
        }
    }

    _onInterfaceToggleFullscreenClicked(isFsRequested)
    {
        if(this.window.isFullscreen === isFsRequested)
            return;

        this.window.toggleFullscreen();
    }

    _onPlayerRealize()
    {
        this.player.widget.disconnect(this._playerRealizeSignal);
        this.player.renderer.expose();

        let display = this.player.widget.get_display();

        this.defaultCursor = Gdk.Cursor.new_from_name(
            display, 'default'
        );
        this.blankCursor = Gdk.Cursor.new_for_display(
            display, Gdk.CursorType.BLANK_CURSOR
        );

        this.playerWindow = this.player.widget.get_window();
        this.setHideCursorTimeout();
    }

    _onPlayerDraw(self, data)
    {
         this.player.widget.disconnect(this._playerDrawSignal);
         this.emit('ready', true);

         if(this.playlist.length)
            this.player.set_playlist(this.playlist);
    }

    _onPlayerStateChanged(self, state)
    {
        if(state === GstPlayer.PlayerState.BUFFERING)
            return;

        let flags = Gtk.ApplicationInhibitFlags.SUSPEND
            | Gtk.ApplicationInhibitFlags.IDLE;

        if(state === GstPlayer.PlayerState.PLAYING) {
            if(this.inhibitCookie)
                return;

            this.inhibitCookie = this.inhibit(
                this.window,
                flags,
                'video is playing'
            );
        }
        else {
            if(!this.inhibitCookie)
                return;

            this.uninhibit(this.inhibitCookie);
            this.inhibitCookie = null;
        }

        debug('set prevent suspend to: ' + this.is_inhibited(flags));
    }

    _onPlayerButtonPressEvent(self, event)
    {
        let [res, button] = event.get_button();
        if(!res) return;

        this.dragStartReady = false;

        switch(button) {
            case Gdk.BUTTON_PRIMARY:
                this._handlePrimaryButtonPress(event, button);
                break;
            case Gdk.BUTTON_SECONDARY:
                if(event.get_event_type() === Gdk.EventType.BUTTON_PRESS)
                    this.player.toggle_play();
                break;
            default:
                break;
        }
    }

    _handlePrimaryButtonPress(event, button)
    {
        let eventType = event.get_event_type();

        switch(eventType) {
            case Gdk.EventType.BUTTON_PRESS:
                let [res, x, y] = event.get_root_coords();
                if(!res)
                    break;
                this.dragStartX = x;
                this.dragStartY = y;
                this.dragStartReady = true;
                break;
            case Gdk.EventType.DOUBLE_BUTTON_PRESS:
                this.window.toggleFullscreen();
                break;
            default:
                break;
        }
    }

    _onPlayerEnterNotifyEvent(self, event)
    {
        this.isCursorInPlayer = true;

        this.setHideCursorTimeout();
        if(this.window.isFullscreen)
            this.setHideControlsTimeout();
    }

    _onPlayerLeaveNotifyEvent(self, event)
    {
        this.isCursorInPlayer = false;

        this.clearTimeout('hideCursor');
        this.clearTimeout('hideControls');
    }

    _onPlayerMotionNotifyEvent(self, event)
    {
        this.playerWindow.set_cursor(this.defaultCursor);
        this.setHideCursorTimeout();

        if(this.window.isFullscreen) {
            if(!this.interface.revealerTop.get_reveal_child()) {
                this.setUpdateTimeInterval();
                this.interface.revealControls(true);
            }
            this.setHideControlsTimeout();
        }
        else if(this.hideControlsTimeout) {
            this.clearTimeout('hideControls');
        }

        if(!this.dragStartReady || this.window.isFullscreen)
            return;

        let [res, x, y] = event.get_root_coords();
        if(!res) return;

        let startDrag = this.player.widget.drag_check_threshold(
            this.dragStartX, this.dragStartY, x, y
        );
        if(!startDrag) return;

        this.dragStartReady = false;
        let timestamp = event.get_time();

        this.window.begin_move_drag(
            Gdk.BUTTON_PRIMARY,
            this.dragStartX,
            this.dragStartY,
            timestamp
        );
    }

    _onPlayerWarning(self, error)
    {
        debug(error.message, 'LEVEL_WARNING');
    }

    _onPlayerError(self, error)
    {
        debug(error);
    }
});
