const { GObject, Gtk } = imports.gi;

var FileChooser = GObject.registerClass(
class ClapperFileChooser extends Gtk.FileChooserNative
{
    _init(window, appName)
    {
        super._init({
            transient_for: window,
            modal: true,
        });

        let filter = new Gtk.FileFilter({
            name: 'Media Files',
        });
        filter.add_mime_type('video/*');
        filter.add_mime_type('audio/*');
        filter.add_mime_type('application/claps');
        this.add_filter(filter);

        this.responseSignal = this.connect('response', this._onResponse.bind(this));
    }

    present()
    {
        /* File chooser closes itself when nobody is holding its ref */
        this.ref();

        super.show();
    }

    _onResponse(filechooser, response)
    {
        this.disconnect(this.responseSignal);
        this.responseSignal = null;

        if(response === Gtk.ResponseType.ACCEPT) {
            let file = this.get_file();
            let { player } = this.get_transient_for().get_child();

            player.set_media(file.get_uri());
        }

        this.unref();
        this.run_dispose();
    }
});
