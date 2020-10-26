const { GObject, Gtk } = imports.gi;
const PrefsBase = imports.clapper_src.prefsBase;

let GeneralPage = GObject.registerClass(
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
    }

    _onVolumeInitialChanged(spinButton, comboBox)
    {
        let value = comboBox.get_active_id();
        spinButton.set_visible(value === 'custom');
    }
});

let BehaviourPage = GObject.registerClass(
class ClapperBehaviourPage extends PrefsBase.Grid
{
    _init()
    {
        super._init();

        this.addTitle('Seeking');
        this.addComboBoxText('Mode', [
            ['normal', "Normal"],
            ['accurate', "Accurate"],
            /* Needs gstplayer pipeline ref count fix */
            //['fast', "Fast"],
        ], 'seeking-mode');
        this.addComboBoxText('Unit', [
            ['second', "Second"],
            ['minute', "Minute"],
            ['percentage', "Percentage"],
        ], 'seeking-unit');
        this.addSpinButton('Value', 1, 99, 'seeking-value');
    }
});

let GStreamerPage = GObject.registerClass(
class ClapperGStreamerPage extends PrefsBase.Grid
{
    _init()
    {
        super._init();

        let label;
        let widget;

        label = this.getLabel('Plugin Ranking', true);
        this.addToGrid(label);
    }
});

function buildPrefsWidget()
{
    let pages = [
        {
            title: 'Player',
            pages: [
                {
                    title: 'General',
                    widget: GeneralPage,
                },
                {
                    title: 'Behaviour',
                    widget: BehaviourPage,
                }
            ]
        },
/*
        {
            title: 'Advanced',
            pages: [
                {
                    title: 'GStreamer',
                    widget: GStreamerPage,
                }
            ]
        }
*/
    ];

    let prefsNotebook = new PrefsBase.Notebook(pages);
    prefsNotebook.add_css_class('prefsnotebook');

    return prefsNotebook;
}
