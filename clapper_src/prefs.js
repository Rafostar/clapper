const { GObject, Gtk } = imports.gi;
const PrefsBase = imports.clapper_src.prefsBase;

let GeneralPage = GObject.registerClass(
class ClapperGeneralPage extends PrefsBase.Grid
{
    _init()
    {
        super._init();

        let label;
        let widget;

        label = this.getLabel('Seeking', true);
        this.addToGrid(label);

        label = this.getLabel('Mode:');
        widget = this.getComboBoxText([
            ['normal', "Normal"],
            ['accurate', "Accurate"],
            /* Needs gstplayer pipeline ref count fix */
            //['fast', "Fast"],
        ], 'seeking-mode');
        this.addToGrid(label, widget);

        label = this.getLabel('Value:');
        widget = this.getSpinButton(1, 99, 'seeking-value');
        this.addToGrid(label, widget);

        label = this.getLabel('Unit:');
        widget = this.getComboBoxText([
            ['second', "Second"],
            ['minute', "Minute"],
            ['percentage', "Percentage"],
        ], 'seeking-unit');
        this.addToGrid(label, widget);
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

var Prefs = GObject.registerClass(
class ClapperPrefs extends Gtk.Box
{
    _init()
    {
        super._init({
            orientation: Gtk.Orientation.VERTICAL,
        });

        this.add_css_class('prefsbox');

        let pages = [
            {
                title: 'General',
                widget: GeneralPage,
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

        this.append(prefsNotebook);
    }
});
