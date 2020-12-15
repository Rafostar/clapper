const { GObject, Gtk } = imports.gi;
const { PlayerRemote } = imports.clapper_src.playerRemote;

var WidgetRemote = GObject.registerClass(
class ClapperWidgetRemote extends Gtk.Grid
{
    _init(opts)
    {
        super._init();

        this.player = new PlayerRemote();
    }
});
