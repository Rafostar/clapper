const { GLib, GObject, Gtk, Pango } = imports.gi;

var HeaderBar = GObject.registerClass(
class ClapperHeaderBar extends Gtk.HeaderBar
{
    _init(window, startButtons, endButtons)
    {
        super._init({
            can_focus: false,
        });

        this.set_title_widget(this._createWidgetForWindow(window));
        startButtons.forEach(btn => this.pack_start(btn));
        endButtons.forEach(btn => this.pack_end(btn));
    }

    updateHeaderBar(mediaInfo)
    {
        let title = mediaInfo.get_title();
        let subtitle = mediaInfo.get_uri() || null;

        if(subtitle && subtitle.startsWith('file://')) {
            subtitle = GLib.filename_from_uri(subtitle)[0];
            subtitle = GLib.path_get_basename(subtitle);
        }

        if(!title) {
            title = (!subtitle)
                ? this.defaultTitle
                : (subtitle.includes('.'))
                ? subtitle.split('.').slice(0, -1).join('.')
                : subtitle;

            subtitle = null;
        }

        this.titleLabel.label = title;
        this.subtitleLabel.visible = (subtitle !== null);

        if(subtitle)
            this.subtitleLabel.label = subtitle;
    }

    _createWidgetForWindow(window)
    {
        let box = new Gtk.Box ({
            orientation: Gtk.Orientation.VERTICAL,
            valign: Gtk.Align.CENTER,
        });

        this.titleLabel = new Gtk.Label({
            halign: Gtk.Align.CENTER,
            single_line_mode: true,
            ellipsize: Pango.EllipsizeMode.END,
            width_chars: 5,
        });
        this.titleLabel.add_css_class('title');
        this.titleLabel.set_parent(box);

        window.bind_property('title', this.titleLabel, 'label',
            GObject.BindingFlags.SYNC_CREATE
        );

        this.subtitleLabel = new Gtk.Label({
            halign: Gtk.Align.CENTER,
            single_line_mode: true,
            ellipsize: Pango.EllipsizeMode.END,
        });
        this.subtitleLabel.add_css_class('subtitle');
        this.subtitleLabel.set_parent(box);
        this.subtitleLabel.visible = false;

        return box;
    }
});
