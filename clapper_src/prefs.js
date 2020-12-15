const { GObject, Gst, Gtk, Pango } = imports.gi;
const Misc = imports.clapper_src.misc;
const PrefsBase = imports.clapper_src.prefsBase;

let { settings } = Misc;

/* PlayFlags are not exported through GI */
Gst.PlayFlags = {
  VIDEO: 1,
  AUDIO: 2,
  TEXT: 4,
  VIS: 8,
  SOFT_VOLUME: 16,
  NATIVE_AUDIO: 32,
  NATIVE_VIDEO: 64,
  DOWNLOAD: 128,
  BUFFERING: 256,
  DEINTERLACE: 512,
  SOFT_COLORBALANCE: 1024,
  FORCE_FILTERS: 2048,
  FORCE_SW_DECODERS: 4096,
};

var GeneralPage = GObject.registerClass(
class ClapperGeneralPage extends PrefsBase.Grid
{
    _init()
    {
        super._init();

        this.addTitle('Startup');
        this.addCheckButton('Auto enter fullscreen', 'fullscreen-auto');

        this.addTitle('Volume');
        let comboBox = this.addComboBoxText('Initial value', [
            ['restore', "Restore"],
            ['custom', "Custom"],
        ], 'volume-initial');
        let spinButton = this.addSpinButton('Value (percentage)', 0, 200, 'volume-value');
        this._onVolumeInitialChanged(spinButton, comboBox);
        comboBox.connect('changed', this._onVolumeInitialChanged.bind(this, spinButton));

        this.addTitle('Finish');
        this.addCheckButton('Close after playback', 'close-auto');
    }

    _onVolumeInitialChanged(spinButton, comboBox)
    {
        let value = comboBox.get_active_id();
        spinButton.set_visible(value === 'custom');
    }
});

var BehaviourPage = GObject.registerClass(
class ClapperBehaviourPage extends PrefsBase.Grid
{
    _init()
    {
        super._init();

        this.addTitle('Seeking');
        this.addComboBoxText('Mode', [
            ['normal', "Normal"],
            ['accurate', "Accurate"],
            ['fast', "Fast"],
        ], 'seeking-mode');
        this.addComboBoxText('Unit', [
            ['second', "Second"],
            ['minute', "Minute"],
            ['percentage', "Percentage"],
        ], 'seeking-unit');
        this.addSpinButton('Value', 1, 99, 'seeking-value');
    }
});

var AudioPage = GObject.registerClass(
class ClapperAudioPage extends PrefsBase.Grid
{
    _init()
    {
        super._init();

        this.addTitle('Synchronization');
        this.addSpinButton('Offset (milliseconds)', -1000, 1000, 'audio-offset', 25);

        this.addTitle('Processing');
        this.addPlayFlagCheckButton('Only use native audio formats', Gst.PlayFlags.NATIVE_AUDIO);
    }
});

var SubtitlesPage = GObject.registerClass(
class ClapperSubtitlesPage extends PrefsBase.Grid
{
    _init()
    {
        super._init();

        /* FIXME: This should be moved to subtitles popup and displayed only when
           external subtitles were added for easier customization per video. */
        //this.addTitle('Synchronization');
        //this.addSpinButton('Offset (milliseconds)', -5000, 5000, 'subtitle-offset', 25);

        this.addTitle('External Subtitles');
        this.addFontButton('Default font', 'subtitle-font');
    }
});

var NetworkPage = GObject.registerClass(
class ClapperNetworkPage extends PrefsBase.Grid
{
    _init()
    {
        super._init();

        this.addTitle('Client');
        this.addPlayFlagCheckButton('Progressive download buffering', Gst.PlayFlags.DOWNLOAD);

        this.addTitle('Server');
        let webServer = this.addCheckButton('Control player remotely', 'webserver-enabled');
        let serverPort = this.addSpinButton('Listening port', 1024, 65535, 'webserver-port');
        webServer.bind_property('active', serverPort, 'visible', GObject.BindingFlags.SYNC_CREATE);
        //let webApp = this.addCheckButton('Run built-in web application', 'webapp-enabled');
        //webServer.bind_property('active', webApp, 'visible', GObject.BindingFlags.SYNC_CREATE);
    }
});

var GStreamerPage = GObject.registerClass(
class ClapperGStreamerPage extends PrefsBase.Grid
{
    _init()
    {
        super._init();

        this.addTitle('Plugin Ranking');
        let listStore = new Gtk.ListStore();
        listStore.set_column_types([
            GObject.TYPE_BOOLEAN,
            GObject.TYPE_STRING,
            GObject.TYPE_STRING,
        ]);
        let treeView = new Gtk.TreeView({
            hexpand: true,
            vexpand: true,
            enable_search: false,
            model: listStore,
        });
        let treeSelection = treeView.get_selection();

        let apply = new Gtk.TreeViewColumn({
            title: "Apply",
        });
        let name = new Gtk.TreeViewColumn({
            title: "Plugin",
            expand: true,
        });
        let rank = new Gtk.TreeViewColumn({
            title: "Rank",
            min_width: 90,
        });

        let applyCell = new Gtk.CellRendererToggle();
        let nameCell = new Gtk.CellRendererText({
            editable: true,
            placeholder_text: "Insert plugin name",
        });
        let rankCell = new Gtk.CellRendererText({
            editable: true,
            weight: Pango.Weight.BOLD,
            placeholder_text: "Insert plugin rank",
        });

        apply.pack_start(applyCell, true);
        name.pack_start(nameCell, true);
        rank.pack_start(rankCell, true);

        apply.add_attribute(applyCell, 'active', 0);
        name.add_attribute(nameCell, 'text', 1);
        rank.add_attribute(rankCell, 'text', 2);

        treeView.insert_column(apply, 0);
        treeView.insert_column(name, 1);
        treeView.insert_column(rank, 2);

        let frame = new Gtk.Frame({
            child: treeView
        });
        this.addToGrid(frame);

        let addButton = new Gtk.Button({
            icon_name: 'list-add-symbolic',
            halign: Gtk.Align.END,
        });
        let removeButton = new Gtk.Button({
            icon_name: 'list-remove-symbolic',
            sensitive: false,
            halign: Gtk.Align.END,
        });
        let label = new Gtk.Label({
            label: 'Changes require player restart',
            halign: Gtk.Align.START,
            hexpand: true,
            ellipsize: Pango.EllipsizeMode.END,
        });
        let box = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
            spacing: 6,
            hexpand: true,
        });
        box.append(label);
        box.append(removeButton);
        box.append(addButton);
        this.addToGrid(box);

        applyCell.connect('toggled', this._onApplyCellEdited.bind(this));
        nameCell.connect('edited', this._onNameCellEdited.bind(this));
        rankCell.connect('edited', this._onRankCellEdited.bind(this));
        addButton.connect('clicked', this._onAddButtonClicked.bind(this, listStore));
        removeButton.connect('clicked', this._onRemoveButtonClicked.bind(this, listStore));
        treeSelection.connect('changed', this._onTreeSelectionChanged.bind(this, removeButton));

        this.settingsChangedSignal = settings.connect(
            'changed::plugin-ranking', this.refreshListStore.bind(this, listStore)
        );

        this.refreshListStore(listStore);
    }

    refreshListStore(listStore)
    {
        let data = JSON.parse(settings.get_string('plugin-ranking'));
        listStore.clear();

        for(let plugin of data) {
            listStore.set(
                listStore.append(),
                [0, 1, 2], [
                    plugin.apply || false,
                    plugin.name || '',
                    plugin.rank || 0
                ]
            );
        }
    }

    updatePlugin(index, prop, value)
    {
        let data = JSON.parse(settings.get_string('plugin-ranking'));
        data[index][prop] = value;
        settings.set_string('plugin-ranking', JSON.stringify(data));
    }

    _onTreeSelectionChanged(removeButton, treeSelection)
    {
        let [isSelected, model, iter] = treeSelection.get_selected();
        this.activeIndex = -1;

        if(isSelected) {
            this.activeIndex = Number(model.get_string_from_iter(iter));
        }

        removeButton.set_sensitive(this.activeIndex >= 0);
    }

    _onAddButtonClicked(listStore, button)
    {
        let data = JSON.parse(settings.get_string('plugin-ranking'));
        data.push({
            apply: false,
            name: '',
            rank: 0,
        });
        settings.set_string('plugin-ranking', JSON.stringify(data));
    }

    _onRemoveButtonClicked(listStore, button)
    {
        if(this.activeIndex < 0)
            return;

        let data = JSON.parse(settings.get_string('plugin-ranking'));
        data.splice(this.activeIndex, 1);
        settings.set_string('plugin-ranking', JSON.stringify(data));
    }

    _onApplyCellEdited(cell, path)
    {
        let newState = !cell.active;
        this.updatePlugin(path, 'apply', newState);
    }

    _onNameCellEdited(cell, path, newText)
    {
        newText = newText.trim();
        this.updatePlugin(path, 'name', newText);
    }

    _onRankCellEdited(cell, path, newText)
    {
        newText = newText.trim();

        if(isNaN(newText))
            newText = 0;

        this.updatePlugin(path, 'rank', Number(newText));
    }

    _onClose()
    {
        super._onClose('gstreamer');

        settings.disconnect(this.settingsChangedSignal);
        this.settingsChangedSignal = null;
    }
});

var TweaksPage = GObject.registerClass(
class ClapperTweaksPage extends PrefsBase.Grid
{
    _init()
    {
        super._init();

        this.addTitle('Appearance');
        let darkCheck = this.addCheckButton('Enable dark theme', 'dark-theme');
        let brighterCheck = this.addCheckButton('Make sliders brighter', 'brighter-sliders');
        this._onDarkThemeToggled(brighterCheck, darkCheck);
        darkCheck.connect('toggled', this._onDarkThemeToggled.bind(this, brighterCheck));

        this.addTitle('Performance');
        this.addCheckButton('Render window shadows', 'render-shadows');
    }

    _onDarkThemeToggled(brighterCheck, darkCheck)
    {
        let isActive = darkCheck.get_active();
        brighterCheck.set_visible(isActive);
    }
});
