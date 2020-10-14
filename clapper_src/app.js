const { Gdk, GLib, GObject, Gtk, GstPlayer } = imports.gi;
const Debug = imports.clapper_src.debug;
const { HeaderBar } = imports.clapper_src.headerbar;
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
        this.dragAllowed = false;

        this.posX = 0;
        this.posY = 0;
    }

    vfunc_startup()
    {
        super.vfunc_startup();
        this.window = new Window(this, APP_NAME);

        this.windowRealizeSignal = this.window.connect(
            'realize', this._onWindowRealize.bind(this)
        );
        this.window.connect(
            'fullscreen-changed', this._onWindowFullscreenChanged.bind(this)
        );
        this.window.connect(
            'close-request', this._onWindowCloseRequest.bind(this)
        );

        this.interface = new Interface();

        let headerStart = [];
        let headerEnd = [
            this.interface.controls.openMenuButton,
            this.interface.controls.fullscreenButton
        ];
        let headerBar = new HeaderBar(this.window, headerStart, headerEnd);
        this.interface.addHeaderBar(headerBar, APP_NAME);

        this.interface.controls.fullscreenButton.connect(
            'clicked', () => this.activeWindow.fullscreen()
        );
        this.interface.controls.unfullscreenButton.connect(
            'clicked', () => this.activeWindow.unfullscreen()
        );

        this.window.set_titlebar(this.interface.headerBar);
        this.window.set_child(this.interface);
    }

    vfunc_activate()
    {
        super.vfunc_activate();

        this.window.present();
        Gtk.StyleContext.add_provider_for_display(
            Gdk.Display.get_default(),
            this.cssProvider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        );
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

            if(this.player.motionController.is_pointer)
                this.player.widget.set_cursor(this.blankCursor);

            return GLib.SOURCE_REMOVE;
        });
    }

    setHideControlsTimeout()
    {
        this.clearTimeout('hideControls');
        this.hideControlsTimeout = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 3, () => {
            this.hideControlsTimeout = null;

            if(this.window.isFullscreen && this.player.motionController.is_pointer) {
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

    _onWindowRealize()
    {
        this.window.disconnect(this.windowRealizeSignal);

        this.player = new Player();

        if(!this.player.widget)
            return this.quit();

        this.player.widget.width_request = 960;
        this.player.widget.height_request = 540;
/*
        this.player.widget.add_events(
            Gdk.EventMask.SCROLL_MASK
            | Gdk.EventMask.ENTER_NOTIFY_MASK
            | Gdk.EventMask.LEAVE_NOTIFY_MASK
        );
*/
        this.interface.addPlayer(this.player);
        this.player.connect('state-changed', this._onPlayerStateChanged.bind(this));

        this.player.clickGesture.connect(
            'pressed', this._onPlayerPressed.bind(this)
        );
        this.player.keyController.connect(
            'key-pressed', this._onPlayerKeyPressed.bind(this)
        );
        this.player.motionController.connect(
            'enter', this._onPlayerEnter.bind(this)
        );
        this.player.motionController.connect(
            'leave', this._onPlayerLeave.bind(this)
        );
        this.player.motionController.connect(
            'motion', this._onPlayerMotion.bind(this)
        );
        this.player.dragGesture.connect(
            'drag-update', this._onPlayerDragUpdate.bind(this)
        );

        /* Widget signals that are disconnected after first run */
        this._playerRealizeSignal = this.player.widget.connect(
            'realize', this._onPlayerRealize.bind(this)
        );
        this._playerMapSignal = this.player.widget.connect(
            'map', this._onPlayerMap.bind(this)
        );
    }

    _onWindowFullscreenChanged(window, isFullscreen)
    {
        if(isFullscreen) {
            this.setUpdateTimeInterval();
            this.setHideControlsTimeout();
        }
        else {
            this.clearTimeout('updateTime');
        }

        this.interface.setFullscreenMode(isFullscreen);
    }

    _onPlayerKeyPressed(self, keyval, keycode, state)
    {
        let bool = false;

        switch(keyval) {
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
                this._onWindowCloseRequest();
                break;
            default:
                break;
        }
    }

    _onPlayerRealize()
    {
        this.player.widget.disconnect(this._playerRealizeSignal);
        this.player.renderer.expose();

        this.defaultCursor = Gdk.Cursor.new_from_name('default', null);
        this.blankCursor = Gdk.Cursor.new_from_name('none', null);

        this.setHideCursorTimeout();
    }

    _onPlayerMap(self, data)
    {
         this.player.widget.disconnect(this._playerMapSignal);
         this.emit('ready', true);

         if(this.playlist.length)
            this.player.set_playlist(this.playlist);
    }

    _onPlayerStateChanged(self, state)
    {
        if(state === GstPlayer.PlayerState.BUFFERING)
            return;

        let isInhibited = false;
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
            if(!this.inhibitCookie)
                debug(new Error('could not inhibit session!'));

            isInhibited = (this.inhibitCookie > 0);
        }
        else {
            //if(!this.inhibitCookie)
                return;

            /* Uninhibit seems to be broken as of GTK 3.99.2
            this.uninhibit(this.inhibitCookie);
            this.inhibitCookie = null;
            */
        }

        debug(`set prevent suspend to: ${isInhibited}`);
    }

    _onPlayerPressed(gesture, nPress, x, y)
    {
        let button = gesture.get_current_button();
        let isDouble = (nPress % 2 == 0);
        this.dragAllowed = !isDouble;

        switch(button) {
            case Gdk.BUTTON_PRIMARY:
                if(isDouble)
                    this.window.toggleFullscreen();
                break;
            case Gdk.BUTTON_SECONDARY:
                this.player.toggle_play();
                break;
            default:
                break;
        }
    }

    _onPlayerEnter(controller, x, y)
    {
        this.setHideCursorTimeout();
        if(this.window.isFullscreen)
            this.setHideControlsTimeout();
    }

    _onPlayerLeave(controller)
    {
        this.clearTimeout('hideCursor');
        this.clearTimeout('hideControls');
    }

    _onPlayerMotion(self, posX, posY)
    {
        /* GTK4 sometimes generates motions with same coords */
        if(this.posX === posX && this.posY === posY)
            return;

        this.posX = posX;
        this.posY = posY;

        this.player.widget.set_cursor(this.defaultCursor);
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
    }

    _onPlayerDragUpdate(gesture, offsetX, offsetY)
    {
        if(!this.dragAllowed || this.activeWindow.isFullscreen)
            return;

        let { gtk_double_click_distance } = this.player.widget.get_settings();

        if (
            Math.abs(offsetX) > gtk_double_click_distance
            || Math.abs(offsetY) > gtk_double_click_distance
        ) {
            let [isActive, startX, startY] = gesture.get_start_point();
            if(!isActive) return;

            this.activeWindow.get_surface().begin_move(
                gesture.get_device(),
                gesture.get_current_button(),
                startX,
                startY,
                gesture.get_current_event_time()
            );

            gesture.reset();
        }
    }

    _onWindowCloseRequest()
    {
        this.window.destroy();
        this.player.widget.emit('destroy');
        this.interface.emit('destroy');

        this.quit();
    }
});
