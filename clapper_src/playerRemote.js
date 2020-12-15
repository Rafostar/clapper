const { GObject } = imports.gi;
const Misc = imports.clapper_src.misc;
const { WebClient } = imports.clapper_src.webClient;

let { settings } = Misc;

var PlayerRemote = GObject.registerClass(
class ClapperPlayerRemote extends GObject.Object
{
    _init()
    {
        super._init();

        this.webclient = new WebClient(settings.get_int('webserver-port'));
    }

    set_playlist(playlist)
    {
        this.webclient.sendMessage({
            action: 'set_playlist',
            value: playlist
        });
    }
});
