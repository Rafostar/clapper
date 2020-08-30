const { GLib, GObject, Gst, GstPlayer } = imports.gi;

var GtkPlayer = GObject.registerClass(
class GtkPlayer extends GstPlayer.Player
{
    _init()
    {
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

        this.loop = GLib.MainLoop.new(null, false);
        this.widget = gtkglsink.widget;
        this.state = GstPlayer.PlayerState.STOPPED;

        this.connect('state_changed', this._onStateChanged.bind(this));
        this.connect('uri_loaded', this._onUriLoaded.bind(this));
        this.widget.connect('destroy', this._onWidgetDestroy.bind(this));
    }

    _onStateChanged(player, state)
    {
        this.state = state;

        if(
            this.state === GstPlayer.PlayerState.STOPPED
            && this.loop.is_running()
        )
            this.loop.quit();
    }

    _onUriLoaded()
    {
        this.play();

        if(!this.loop.is_running())
            this.loop.run();
    }

    _onWidgetDestroy()
    {
        if(this.state !== GstPlayer.PlayerState.STOPPED)
            this.stop();
    }
});
