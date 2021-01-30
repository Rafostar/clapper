const { Gdk, GLib, GObject, Gst, Gtk, Pango } = imports.gi;

var PlaylistWidget = GObject.registerClass(
class ClapperPlaylistWidget extends Gtk.ListBox
{
    _init()
    {
        super._init({
            selection_mode: Gtk.SelectionMode.NONE,
        });
        this.activeRowId = -1;
        this.connect('row-activated', this._onRowActivated.bind(this));
    }

    addItem(uri)
    {
        const item = new PlaylistItem(uri);
        this.append(item);
    }

    removeItem(item)
    {
        const itemIndex = item.get_index();

        if(itemIndex === this.activeRowId) {
            const root = this.get_root();
            root.emit('close-request');

            return;
        }

        if(itemIndex < this.activeRowId)
            this.activeRowId--;

        this.remove(item);
    }

    removeAll()
    {
        let oldItem;
        while((oldItem = this.get_row_at_index(0)))
            this.remove(oldItem);

        this.activeRowId = -1;
    }

    nextTrack()
    {
        const nextRow = this.get_row_at_index(this.activeRowId + 1);
        if(!nextRow)
            return false;

        nextRow.activate();

        return true;
    }

    getActiveFilename()
    {
        const row = this.get_row_at_index(this.activeRowId);
        if(!row) return null;

        return row.filename;
    }

    /* FIXME: Remove once/if GstPlay(er) gets
     * less vague MediaInfo signals */
    getActiveIsLocalFile()
    {
        const row = this.get_row_at_index(this.activeRowId);
        if(!row) return null;

        return row.isLocalFile;
    }

    deactivateActiveItem()
    {
        if(this.activeRowId < 0)
            return;

        const row = this.get_row_at_index(this.activeRowId);
        if(!row) return null;

        const icon = row.child.get_first_child();
        const button = row.child.get_last_child();

        icon.icon_name = 'open-menu-symbolic';
        button.icon_name = 'list-remove-symbolic';
    }

    _onRowActivated(listBox, row)
    {
        const { player } = this.get_ancestor(Gtk.Grid);
        const icon = row.child.get_first_child();
        const button = row.child.get_last_child();

        this.deactivateActiveItem();
        icon.icon_name = 'media-playback-start-symbolic';
        button.icon_name = 'window-close-symbolic';

        this.activeRowId = row.get_index();
        player.set_uri(row.uri);
    }
});

let PlaylistItem = GObject.registerClass(
class ClapperPlaylistItem extends Gtk.ListBoxRow
{
    _init(uri)
    {
        super._init({
            /* TODO: Fix playlist navigation in fullscreen */
            can_focus: false,
        });

        this.uri = uri;
        this.isLocalFile = false;

        let filename;
        if(Gst.Uri.get_protocol(uri) === 'file') {
            filename = GLib.path_get_basename(
                GLib.filename_from_uri(uri)[0]
            );
            this.isLocalFile = true;
        }
        this.filename = filename || uri;

        const box = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
            spacing: 6,
            margin_start: 6,
            margin_end: 6,
            height_request: 22,
        });
        const icon = new Gtk.Image({
            icon_name: 'open-menu-symbolic',
        });
        const label = new Gtk.Label({
            label: this.filename,
            single_line_mode: true,
            ellipsize: Pango.EllipsizeMode.END,
            width_chars: 5,
            hexpand: true,
            halign: Gtk.Align.START,
        });
        const button = new Gtk.Button({
            icon_name: 'list-remove-symbolic',
        });
        button.add_css_class('flat');
        button.add_css_class('circular');
        button.add_css_class('popoverbutton');
        button.connect('clicked', this._onRemoveClicked.bind(this));

        box.append(icon);
        box.append(label);
        box.append(button);
        this.set_child(box);

/* FIXME: D&D inside popover is broken in GTK4
        const dragSource = new Gtk.DragSource({
            actions: Gdk.DragAction.MOVE
        });
        dragSource.connect('prepare', this._onDragPrepare.bind(this));
        dragSource.connect('drag-begin', this._onDragBegin.bind(this));
        dragSource.connect('drag-end', this._onDragEnd.bind(this));
        this.add_controller(dragSource);

        const dropTarget = new Gtk.DropTarget({
            actions: Gdk.DragAction.MOVE,
            preload: true,
        });
        dropTarget.set_gtypes([PlaylistItem]);
        dropTarget.connect('enter', this._onEnter.bind(this));
        dropTarget.connect('drop', this._onDrop.bind(this));
        this.add_controller(dropTarget);
*/
    }

    _onRemoveClicked(button)
    {
        const listBox = this.get_ancestor(Gtk.ListBox);

        listBox.removeItem(this);
    }

    _onDragPrepare(source, x, y)
    {
        const widget = source.get_widget();
        const paintable = new Gtk.WidgetPaintable({ widget });
        const staticImg = paintable.get_current_image();

        source.set_icon(staticImg, x, y);

        return Gdk.ContentProvider.new_for_value(widget);
    }

    _onDragBegin(source, drag)
    {
        this.child.set_opacity(0.3);
    }

    _onDragEnd(source, drag, deleteData)
    {
        this.child.set_opacity(1.0);
    }

    _onEnter(target, x, y)
    {
        return (target.value)
            ? Gdk.DragAction.MOVE
            : 0;
    }

    _onDrop(target, value, x, y)
    {
        const destIndex = this.get_index();
        const targetIndex = value.get_index();

        if(destIndex === targetIndex)
            return true;

        const listBox = this.get_ancestor(Gtk.ListBox);

        if(listBox && destIndex >= 0) {
            listBox.remove(value);
            listBox.insert(value, destIndex);

            return true;
        }

        return false;
    }
});
