const { Gdk, GLib, GObject, Gtk, Pango } = imports.gi;
const Debug = imports.src.debug;
const Misc = imports.src.misc;

const { debug, warn } = Debug;

var RepeatMode = {
    NONE: 0,
    TRACK: 1,
    PLAYLIST: 2,
    SHUFFLE: 3,
};

const repeatIcons = [
    'media-playlist-consecutive-symbolic',
    'media-playlist-repeat-song-symbolic',
    'media-playlist-repeat-symbolic',
    'media-playlist-shuffle-symbolic',
];

var PlaylistWidget = GObject.registerClass({
    GTypeName: 'ClapperPlaylistWidget',
},
class ClapperPlaylistWidget extends Gtk.ListBox
{
    _init()
    {
        super._init({
            selection_mode: Gtk.SelectionMode.NONE,
        });
        this.activeRowId = -1;
        this.repeatMode = RepeatMode.NONE;
        this.add_css_class('clapperplaylist');

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
            this.activate_action('window.close', null);
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
        return this._switchTrack(false);
    }

    prevTrack()
    {
        return this._switchTrack(true);
    }

    getActiveRow()
    {
        return this.get_row_at_index(this.activeRowId);
    }

    getPlaylist(useFilePaths)
    {
        const playlist = [];
        let index = 0;
        let item;

        while((item = this.get_row_at_index(index))) {
            const path = (useFilePaths && item.isLocalFile)
                ? GLib.filename_from_uri(item.uri)[0]
                : item.uri;

            playlist.push(path);
            index++;
        }

        return playlist;
    }

    getActiveFilename()
    {
        const row = this.getActiveRow();
        if(!row) return null;

        return row.filename;
    }

    changeActiveRow(rowId)
    {
        const row = this.get_row_at_index(rowId);
        if(!row)
            return false;

        row.activate();

        return true;
    }

    changeRepeatMode(mode)
    {
        const lastMode = Object.keys(RepeatMode).length - 1;
        const row = this.getActiveRow();
        if(!row) return null;

        if(mode < 0 || mode > lastMode) {
            warn(`ignored invalid repeat mode value: ${mode}`);
            return;
        }

        if(mode >= 0)
            this.repeatMode = mode;
        else {
            this.repeatMode++;
            if(this.repeatMode > lastMode)
                this.repeatMode = 0;
        }

        const repeatButton = row.child.get_first_child();
        repeatButton.icon_name = repeatIcons[this.repeatMode];

        debug(`set repeat mode: ${this.repeatMode}`);
    }

    _deactivateActiveItem(isRemoveChange)
    {
        if(this.activeRowId < 0)
            return;

        const row = this.getActiveRow();
        if(!row) return null;

        const repeatButton = row.child.get_first_child();
        repeatButton.sensitive = false;
        repeatButton.icon_name = 'open-menu-symbolic';

        if(isRemoveChange) {
            const removeButton = row.child.get_last_child();
            removeButton.icon_name = 'list-remove-symbolic';
        }
    }

    _switchTrack(isPrevious)
    {
        const rowId = (isPrevious)
            ? this.activeRowId - 1
            : this.activeRowId + 1;

        return this.changeActiveRow(rowId);
    }

    _onRowActivated(listBox, row)
    {
        const { player } = this.get_ancestor(Gtk.Grid);
        const repeatButton = row.child.get_first_child();
        const removeButton = row.child.get_last_child();

        this._deactivateActiveItem(true);
        repeatButton.sensitive = true;
        repeatButton.icon_name = repeatIcons[this.repeatMode];
        removeButton.icon_name = 'window-close-symbolic';

        this.activeRowId = row.get_index();
        player.set_uri(row.uri);
    }

    _handleStreamEnded(player)
    {
        /* Seek to beginning when repeating track
         * or playlist with only one item */
        if(
            this.repeatMode === RepeatMode.TRACK
            || (this.repeatMode !== RepeatMode.NONE
            && this.activeRowId === 0
            && !this.get_row_at_index(1))
        ) {
            debug('seeking to beginning');

            player.seek(0);
            return true;
        }

        if(this.repeatMode === RepeatMode.SHUFFLE) {
            const playlistIds = [];
            let index = 0;

            debug('selecting random playlist item');

            while(this.get_row_at_index(index)) {
                /* We prefer to not repeat the same track */
                if(index !== this.activeRowId)
                    playlistIds.push(index);

                index++;
            }

            /* We always have non-empty array here,
             * otherwise seek to beginning is performed */
            const randomId = playlistIds[
                Math.floor(Math.random() * playlistIds.length)
            ];
            debug(`selected random playlist item: ${randomId}`);

            return this.changeActiveRow(randomId);
        }

        if(this.nextTrack())
            return true;

        if(this.repeatMode === RepeatMode.PLAYLIST)
            return this.changeActiveRow(0);

        this._deactivateActiveItem(false);

        return false;
    }
});

let PlaylistItem = GObject.registerClass({
    GTypeName: 'ClapperPlaylistItem',
},
class ClapperPlaylistItem extends Gtk.ListBoxRow
{
    _init(uri)
    {
        super._init({
            can_focus: false,
        });

        this.uri = uri;
        this.isLocalFile = false;

        let filename;
        if(Misc.getUriProtocol(uri) === 'file') {
            filename = GLib.path_get_basename(
                GLib.filename_from_uri(uri)[0]
            );
            this.isLocalFile = true;
        }
        this.filename = filename || uri;
        this.set_tooltip_text(this.filename);

        const box = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
            spacing: 6,
            margin_start: 6,
            margin_end: 6,
            height_request: 22,
        });
        const repeatButton = new Gtk.Button({
            icon_name: 'open-menu-symbolic',
            sensitive: false,
        });
        repeatButton.add_css_class('flat');
        repeatButton.add_css_class('circular');
        repeatButton.connect('clicked', this._onRepeatClicked.bind(this));
        const label = new Gtk.Label({
            label: this.filename,
            single_line_mode: true,
            ellipsize: Pango.EllipsizeMode.END,
            width_chars: 5,
            hexpand: true,
            halign: Gtk.Align.START,
        });
        const removeButton = new Gtk.Button({
            icon_name: 'list-remove-symbolic',
        });
        removeButton.add_css_class('flat');
        removeButton.add_css_class('circular');
        removeButton.connect('clicked', this._onRemoveClicked.bind(this));

        box.append(repeatButton);
        box.append(label);
        box.append(removeButton);
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

    _onRepeatClicked(button)
    {
        const listBox = this.get_ancestor(Gtk.ListBox);

        listBox.changeRepeatMode();
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
