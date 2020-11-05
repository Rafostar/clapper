const { GObject, Gtk, Gst } = imports.gi;

var UriDialog = GObject.registerClass(
class ClapperUriDialog extends Gtk.Dialog
{
    _init(window, appName)
    {
        super._init({
            transient_for: window,
            title: 'Open URI',
            default_width: 460,
            modal: true,
        });

        let box = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
            valign: Gtk.Align.CENTER,
            spacing: 6,
        });
        box.add_css_class('uridialogbox');

        let linkEntry = new Gtk.Entry({
            activates_default: true,
            truncate_multiline: true,
            width_request: 220,
            height_request: 36,
            hexpand: true,
        });
        linkEntry.set_placeholder_text("Enter or drop URI here");
        linkEntry.connect('notify::text', this._onTextNotify.bind(this));
        box.append(linkEntry);

        let openButton = new Gtk.Button({
            label: "Open",
            halign: Gtk.Align.END,
            sensitive: false,
        });
        openButton.connect('clicked', this._onOpenButtonClicked.bind(this));
        box.append(openButton);

        this.set_child(box);
    }

    openUri(uri)
    {
        let { player } = this.get_transient_for().get_child();
        player.set_media(uri);

        this.close();
    }

    _onTextNotify(entry)
    {
        let isUriValid = (entry.text.length)
            ? Gst.uri_is_valid(entry.text)
            : false;

        let button = entry.get_next_sibling();
        button.set_sensitive(isUriValid);
    }

    _onOpenButtonClicked(button)
    {
        let entry = button.get_prev_sibling();
        this.openUri(entry.text);
    }
});
