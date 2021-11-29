const { Adw, GObject, Gio, Gst, Gtk } = imports.gi;
const Debug = imports.src.debug;
const Misc = imports.src.misc;

const { debug } = Debug;
const { settings } = Misc;

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

const widgetOpts = {
    halign: Gtk.Align.CENTER,
    valign: Gtk.Align.CENTER,
};

function getCommonProps()
{
    return {
        'schema-name': GObject.ParamSpec.string(
            'schema-name',
            'GSchema setting name',
            'Name of the setting to bind',
            GObject.ParamFlags.WRITABLE,
            null
        ),
    };
}

const flags = Gio.SettingsBindFlags.DEFAULT;

let PrefsActionRow = GObject.registerClass({
    GTypeName: 'ClapperPrefsActionRow',
    Properties: getCommonProps(),
},
class ClapperPrefsActionRow extends Adw.ActionRow
{
    _init(widget)
    {
        super._init();

        this._schemaName = null;
        this._bindProp = null;

        if(widget) {
            this.add_suffix(widget);
            this.set_activatable_widget(widget);
        }
    }

    set schema_name(value)
    {
        this._schemaName = value;
    }

    vfunc_realize()
    {
        super.vfunc_realize();

        if(this._schemaName && this._bindProp) {
            settings.bind(this._schemaName,
                this.activatable_widget, this._bindProp, flags
            );
        }
        this._schemaName = null;
    }
});

let PrefsSubpageRow = GObject.registerClass({
    GTypeName: 'ClapperPrefsSubpageRow',
    Properties: getCommonProps(),
},
class ClapperPrefsSubpageRow extends Adw.ActionRow
{
    _init(widget)
    {
        super._init({
            activatable: true,
        });

        this._prefsSubpage = null;

        const icon = new Gtk.Image({
            icon_name: 'go-next-symbolic',
        });
        this.add_suffix(icon);
    }

    vfunc_activate()
    {
        super.vfunc_activate();

        if(!this._prefsSubpage)
            this._prefsSubpage = this._createSubpage();

        const prefs = this.get_ancestor(PrefsWindow);
        prefs.present_subpage(this._prefsSubpage);
    }

    _createSubpage()
    {
        /* For override */
        return null;
    }
});

GObject.registerClass({
    GTypeName: 'ClapperPrefsSwitch',
    Properties: {
        'custom-icon-name': GObject.ParamSpec.string(
            'custom-icon-name',
            'Icon name',
            'Name of the icon',
            GObject.ParamFlags.WRITABLE | GObject.ParamFlags.CONSTRUCT_ONLY,
            null
        ),
        'custom-icon-subtitle': GObject.ParamSpec.string(
            'custom-icon-subtitle',
            'Icon subtitle',
            'Text below the icon',
            GObject.ParamFlags.WRITABLE | GObject.ParamFlags.CONSTRUCT_ONLY,
            null
        ),
    }
},
class ClapperPrefsSwitch extends PrefsActionRow
{
    _init(opts)
    {
        super._init(null);
        this._bindProp = 'active';

        if(opts.custom_icon_name || opts.custom_icon_subtitle) {
            const box = new Gtk.Box({
                margin_top: 2,
                orientation: Gtk.Orientation.VERTICAL,
                valign: Gtk.Align.CENTER,
            });
            const customIcon = new Gtk.Image({
                icon_name: opts.custom_icon_name || null,
            });
            box.append(customIcon);

            const customLabel = new Gtk.Label({
                label: opts.custom_icon_subtitle || '',
            });
            customLabel.add_css_class('subtitle');
            box.append(customLabel);

            this.add_suffix(box);
        }

        const sw = new Gtk.Switch(widgetOpts);
        this.add_suffix(sw);
        this.set_activatable_widget(sw);
    }
});

GObject.registerClass({
    GTypeName: 'ClapperPrefsPlayFlagSwitch',
    Properties: {
        'play-flag': GObject.ParamSpec.int(
            'play-flag',
            'PlayFlag',
            'Value of the gstreamer play flag to toggle',
            GObject.ParamFlags.WRITABLE,
            1, 4096, 1,
        ),
    },
},
class ClapperPrefsPlayFlagSwitch extends PrefsActionRow
{
    _init()
    {
        super._init(new Gtk.Switch(widgetOpts));

        this._flag = 1;
        this._doneRealize = false;
    }

    set play_flag(value)
    {
        this._flag = value;
    }

    vfunc_realize()
    {
        super.vfunc_realize();

        if(!this._doneRealize) {
            const playFlags = settings.get_int('play-flags');

            this.activatable_widget.active = (
                (playFlags & this._flag) === this._flag
            );
            this.activatable_widget.connect(
                'notify::active', this._onPlayFlagToggled.bind(this)
            );
        }
        this._doneRealize = true;
    }

    _onPlayFlagToggled()
    {
        let playFlags = settings.get_int('play-flags');

        if(this.activatable_widget.active)
            playFlags |= this._flag;
        else
            playFlags &= ~this._flag;

        settings.set_int('play-flags', playFlags);
    }
});

GObject.registerClass({
    GTypeName: 'ClapperPrefsSpin',
    Properties: {
        'spin-adjustment': GObject.ParamSpec.object(
            'spin-adjustment',
            'GtkAdjustment',
            'Custom GtkAdjustment for spin button',
            GObject.ParamFlags.WRITABLE,
            Gtk.Adjustment
        ),
    },
},
class ClapperPrefsSpin extends PrefsActionRow
{
    _init()
    {
        super._init(new Gtk.SpinButton(widgetOpts));
        this._bindProp = 'value';
    }

    set spin_adjustment(value)
    {
        this.activatable_widget.set_adjustment(value);
    }
});

let PrefsPluginFeature = GObject.registerClass({
    GTypeName: 'ClapperPrefsPluginFeature',
},
class ClapperPrefsPluginFeature extends Adw.ActionRow
{
    _init(featureObj)
    {
        super._init({
            title: featureObj.name,
        });

        const enableSwitch = new Gtk.Switch(widgetOpts);
        const spinButton = new Gtk.SpinButton(widgetOpts);

        spinButton.set_range(0, 512);
        spinButton.set_increments(1, 1);

        enableSwitch.active = featureObj.enabled;
        spinButton.value = featureObj.rank;
        this.currentRank = featureObj.rank;

        this.add_suffix(enableSwitch);
        this.add_suffix(spinButton);

        enableSwitch.bind_property('active', spinButton, 'sensitive',
            GObject.BindingFlags.SYNC_CREATE
        );

        enableSwitch.connect('notify::active', this._onSwitchActivate.bind(this));
        spinButton.connect('value-changed', this._onValueChanged.bind(this));
    }

    _updateRanking(data)
    {
        settings.set_string('plugin-ranking', JSON.stringify(data));
    }

    _onSwitchActivate(enableSwitch)
    {
        const { settingsData } = this.get_ancestor(PrefsPluginRankingSubpage);
        const pluginExp = this.get_ancestor(PrefsPluginExpander);

        if(enableSwitch.active) {
            settingsData[this.title] = this.currentRank;
            pluginExp.modCount++;
        }
        else if(settingsData[this.title] != null) {
            delete settingsData[this.title];
            pluginExp.modCount--;
        }

        this._updateRanking(settingsData);
    }

    _onValueChanged(spinButton)
    {
        const { settingsData } = this.get_ancestor(PrefsPluginRankingSubpage);

        this.currentRank = spinButton.value;
        settingsData[this.title] = this.currentRank;

        this._updateRanking(settingsData);
    }
});

GObject.registerClass({
    GTypeName: 'ClapperPrefsFont',
},
class ClapperPrefsFont extends PrefsActionRow
{
    _init()
    {
        const opts = {
            use_font: true,
            use_size: true,
        };
        Object.assign(opts, widgetOpts);

        super._init(new Gtk.FontButton(opts));
        this._bindProp = 'font';
    }
});

GObject.registerClass({
    GTypeName: 'ClapperPrefsCombo',
    Properties: getCommonProps(),
},
class ClapperPrefsCombo extends Adw.ComboRow
{
    _init()
    {
        super._init();
        this._schemaName = null;
    }

    set schema_name(value)
    {
        this._schemaName = value;
    }

    vfunc_realize()
    {
        super.vfunc_realize();

        if(this._schemaName)
            settings.bind(this._schemaName, this, 'selected', flags);

        this._schemaName = null;
    }
});

GObject.registerClass({
    GTypeName: 'ClapperPrefsExpander',
    Properties: getCommonProps(),
},
class ClapperPrefsExpander extends Adw.ExpanderRow
{
    _init()
    {
        super._init({
            show_enable_switch: true,
        });
    }

    set schema_name(value)
    {
        settings.bind(value, this, 'enable-expansion', flags);
    }
});

GObject.registerClass({
    GTypeName: 'ClapperPrefsPluginRankingSubpageRow',
},
class ClapperPrefsPluginRankingSubpageRow extends PrefsSubpageRow
{
    _createSubpage()
    {
        return new PrefsPluginRankingSubpage();
    }
});

let PrefsPluginExpander = GObject.registerClass({
    GTypeName: 'ClapperPrefsPluginExpander',
},
class ClapperPrefsPluginExpander extends Adw.ExpanderRow
{
    _init(plugin, modCount)
    {
        super._init({
            title: plugin,
            show_enable_switch: false,
        });
        this.modCount = modCount;

        this.expandSignal = this.connect(
            'notify::expanded', this._onExpandedNotify.bind(this)
        );
    }

    set modCount(value)
    {
        this._modCount = value;
        this.icon_name = (value > 0) ? 'dialog-information-symbolic' : null;

        debug(`Plugin ${this.title} has ${value} modified features`);
    }

    get modCount()
    {
        return this._modCount;
    }

    _onExpandedNotify()
    {
        if(!this.expanded)
            return;

        this.disconnect(this.expandSignal);
        this.expandSignal = null;

        const { pluginsData } = this.get_ancestor(PrefsPluginRankingSubpage);

        pluginsData[this.title].sort((a, b) =>
            (a.name > b.name) - (a.name < b.name)
        );
        const featuresNames = Object.keys(pluginsData[this.title]);
        debug(`Adding ${featuresNames.length} features to the list of plugin: ${this.title}`);

        for(let featureObj of pluginsData[this.title]) {
            const prefsPluginFeature = new PrefsPluginFeature(featureObj);

            /* TODO: Remove old libadwaita compat */
            if(this.add_row)
                this.add_row(prefsPluginFeature);
            else
                this.add(prefsPluginFeature);
        }
    }
});

let PrefsPluginRankingSubpage = GObject.registerClass({
    GTypeName: 'ClapperPrefsPluginRankingSubpage',
    Template: Misc.getResourceUri('ui/preferences-plugin-ranking-subpage.ui'),
    InternalChildren: ['decoders_group'],
},
class ClapperPrefsPluginRankingSubpage extends Gtk.Box
{
    _init()
    {
        super._init();

        if(!Gst.is_initialized())
            Gst.init(null);

        const gstRegistry = Gst.Registry.get();
        const decoders = gstRegistry.feature_filter(this._decodersFilterCb, false);

        const plugins = {};
        const mods = {};
        this.settingsData = {};

        /* In case someone messed up gsettings values */
        try {
            this.settingsData = JSON.parse(settings.get_string('plugin-ranking'));
            /* Might be an array in older Clapper versions */
            if(Array.isArray(this.settingsData))
                this.settingsData = {};
        }
        catch(err) { /* Ignore */ }

        for(let decoder of decoders) {
            const pluginName = decoder.get_plugin_name();

            /* Do not add unsupported plugins */
            switch(pluginName) {
                case 'playback':
                    continue;
                default:
                    break;
            }

            if(!plugins[pluginName])
                plugins[pluginName] = [];

            const decName = decoder.get_name();
            const isModified = (this.settingsData[decName] != null);

            plugins[pluginName].push({
                name: decName,
                rank: decoder.get_rank(),
                enabled: isModified,
            });

            if(isModified) {
                if(!mods[pluginName])
                    mods[pluginName] = 0;

                mods[pluginName]++;
            }
        }

        const pluginsNames = Object.keys(plugins);
        debug(`Adding ${pluginsNames.length} found plugins to the list`);

        this.pluginsData = pluginsNames.sort().reduce((res, key) =>
            (res[key] = plugins[key], res), {}
        );

        for(let plugin in this.pluginsData) {
            const modCount = mods[plugin] || 0;
            this._decoders_group.add(new PrefsPluginExpander(plugin, modCount));
        }
    }

    _decodersFilterCb(feature)
    {
        return (
            feature.list_is_type
            && feature.list_is_type(Gst.ELEMENT_FACTORY_TYPE_DECODER)
        );
    }

    _onReturnClicked(button)
    {
        const prefs = this.get_ancestor(PrefsWindow);
        prefs.close_subpage();
    }
});

var PrefsWindow = GObject.registerClass({
    GTypeName: 'ClapperPrefsWindow',
    Template: Misc.getResourceUri('ui/preferences-window.ui'),
},
class ClapperPrefsWindow extends Adw.PreferencesWindow
{
    _init(window)
    {
        super._init({
            transient_for: window,
        });

        /* FIXME: old libadwaita compat, should be
         * normally in prefs UI file */
        this.can_swipe_back = true;
        this.can_navigate_back = true;

        this.show();
    }
});
