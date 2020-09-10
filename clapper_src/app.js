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

            if(this.window.isFullscreen && this.isCursorInPlayer)
                this.interface.revealControls(false);

            return GLib.SOURCE_REMOVE;
        });
    }

    clearTimeout(name)
    {
        if(!this[`${name}Timeout`])
            return;

        GLib.source_remove(this[`${name}Timeout`]);
        this[`${name}Timeout`] = null;
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
        this.interface.addHeaderBar(headerBar, APP_NAME);
        this.interface.controls.toggleFullscreenButton.connect(
            'clicked', this._onInterfaceToggleFullscreenClicked.bind(this)
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

        this.player.widget.connect(
            'button-press-event', this._onPlayerButtonPressEvent.bind(this)
        );
        this.player.widget.connect(
            'scroll-event', this._onPlayerScrollEvent.bind(this)
        );
        this.player.widget.connect(
            'enter-notify-event', this._onPlayerEnterNotifyEvent.bind(this)
        );
        this.player.widget.connect(
            'leave-notify-event', this._onPlayerLeaveNotifyEvent.bind(this)
        );
        this.player.widget.connect(
            'motion-notify-event', this._onPlayerMotionNotifyEvent.bind(this)
        );
        this.playerRealizeSignal = this.player.widget.connect(
            'realize', this._onPlayerRealize.bind(this)
        );
        this.playerDrawSignal = this.player.widget.connect(
            'draw', this._onPlayerDraw.bind(this)
        );

        this.player.widget.show();
    }

    _onWindowFullscreenChanged(window, isFullscreen)
    {
        // when changing fullscreen pango layout of popup is lost
        // and we need to re-add marks to the new layout
        this.interface.controls.setVolumeMarks(false);

        this.interface.controls.toggleFullscreenButton.image = (isFullscreen)
            ? this.interface.controls.unfullscreenImage
            : this.interface.controls.fullscreenImage;

        if(isFullscreen) {
            this.interface.showControls(true);
            this.setHideControlsTimeout();
        }
        this.interface.setControlsOnVideo(isFullscreen);
        this.interface.controls.setVolumeMarks(true);
        this.interface.controls.fullscreenMode = isFullscreen;
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
                //this._handleScaleIncrement('position', bool);
                break;
            case Gdk.KEY_Up:
                bool = true;
            case Gdk.KEY_Down:
                this._handleScaleIncrement('volume', bool);
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
                this.quit();
                break;
            default:
                break;
        }
    }

    _onInterfaceToggleFullscreenClicked()
    {
        // we need some way to refresh toggle fullscreen button on click
        // otherwise it does not lose the hover effect after window transition
        // for now hide->transition->show does the job done
        this.interface.controls.toggleFullscreenButton.hide();
        this.window.toggleFullscreen();
        this.interface.controls.toggleFullscreenButton.show();
    }

    _onPlayerRealize()
    {
        this.player.widget.disconnect(this.playerRealizeSignal);
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
         this.player.widget.disconnect(this.playerDrawSignal);
         this.emit('ready', true);

         if(!this.playlist.length)
            return;

         this.player.set_media(this.playlist[0]);
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
                if(event.get_event_type() !== Gdk.EventType.DOUBLE_BUTTON_PRESS)
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

    _onPlayerScrollEvent(self, event)
    {
        let [res, direction] = event.get_scroll_direction();
        if(!res) return;

        let type = 'volume';

        switch(direction) {
            case Gdk.ScrollDirection.RIGHT:
            case Gdk.ScrollDirection.LEFT:
                type = 'position';
            case Gdk.ScrollDirection.UP:
            case Gdk.ScrollDirection.DOWN:
                let isUp = (
                    direction === Gdk.ScrollDirection.UP
                    || direction === Gdk.ScrollDirection.RIGHT
                );
                this._handleScaleIncrement(type, isUp);
                break;
            default:
                break;
        }
    }

    _handleScaleIncrement(type, isUp)
    {
        let value = this.interface.controls[`${type}Scale`].get_value();
        let maxValue = this.interface.controls[`${type}Adjustment`].get_upper();
        let increment = this.interface.controls[`${type}Adjustment`].get_page_increment();

        value += (isUp) ? increment : -increment;
        value = (value < 0)
            ? 0
            : (value > maxValue)
            ? maxValue
            : value;

        this.interface.controls[`${type}Scale`].set_value(value);
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
            this.setHideControlsTimeout();
            this.interface.revealControls(true);
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
