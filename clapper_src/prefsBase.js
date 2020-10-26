const { Gio, GObject, Gtk } = imports.gi;
const Debug = imports.clapper_src.debug;

let { debug } = Debug;

var Notebook = GObject.registerClass(
class ClapperPrefsNotebook extends Gtk.Notebook
{
    _init(pages, isSubpage)
    {
        super._init({
            show_border: false,
            vexpand: true,
            hexpand: true,
        });

        if(isSubpage) {
            this.set_tab_pos(Gtk.PositionType.LEFT);
            this.add_css_class('prefssubpage');
        }

        this.addArrayPages(pages);
    }

    addArrayPages(array)
    {
        for(let obj of array)
            this.addObjectPages(obj);
    }

    addObjectPages(item)
    {
        let widget = (item.pages)
            ? new Notebook(item.pages, true)
            : new item.widget();

        this.addToNotebook(widget, item.title);
    }

    addToNotebook(widget, title)
    {
        let label = new Gtk.Label({
            label: title,
        });
        this.append_page(widget, label);
    }

    _onClose()
    {
        let totalPages = this.get_n_pages();
        let index = 0;

        while(index < totalPages) {
            let page = this.get_nth_page(index);
            page._onClose();
            index++;
        }
    }
});

var Grid = GObject.registerClass(
class ClapperPrefsGrid extends Gtk.Grid
{
    _init()
    {
        super._init({
            row_spacing: 6,
            column_spacing: 20,
        });

        this.settings = new Gio.Settings({
            schema_id: 'com.github.rafostar.Clapper'
        });
        this.flag = Gio.SettingsBindFlags.DEFAULT;

        this.gridIndex = 0;
        this.widgetDefaults = {
            width_request: 160,
            halign: Gtk.Align.END,
            valign: Gtk.Align.CENTER,
        };
    }

    addToGrid(leftWidget, rightWidget)
    {
        let spanWidth = 2;

        if(rightWidget) {
            spanWidth = 1;
            rightWidget.bind_property('visible', leftWidget, 'visible',
                GObject.BindingFlags.SYNC_CREATE
            );
            this.attach(rightWidget, 1, this.gridIndex, 1, 1);
        }

        this.attach(leftWidget, 0, this.gridIndex, spanWidth, 1);
        this.gridIndex++;

        return rightWidget || leftWidget;
    }

    addTitle(text)
    {
        let label = this.getLabel(text, true);

        return this.addToGrid(label);
    }

    addComboBoxText(text, entries, setting)
    {
        let label = this.getLabel(text + ':');
        let widget = this.getComboBoxText(entries, setting);

        return this.addToGrid(label, widget);
    }

    addSpinButton(text, min, max, setting)
    {
        let label = this.getLabel(text + ':');
        let widget = this.getSpinButton(min, max, setting);

        return this.addToGrid(label, widget);
    }

    addCheckButton(text, setting)
    {
        let widget = this.getCheckButton(text, setting);

        return this.addToGrid(widget);
    }

    getLabel(text, isTitle)
    {
        let marginLR = 0;
        let marginTop = (isTitle && this.gridIndex > 0) ? 16 : 0;
        let marginBottom = (isTitle) ? 2 : 0;

        if(isTitle)
            text = '<span font="12"><b>' + text + '</b></span>';
        else
            marginLR = 12;

        return new Gtk.Label({
            label: text,
            use_markup: true,
            hexpand: true,
            halign: Gtk.Align.START,
            margin_top: marginTop,
            margin_bottom: marginBottom,
            margin_start: marginLR,
            margin_end: marginLR,
        });
    }

    getComboBoxText(entries, setting)
    {
        let comboBox = new Gtk.ComboBoxText(this.widgetDefaults);

        for(let entry of entries)
            comboBox.append(entry[0], entry[1]);

        this.settings.bind(setting, comboBox, 'active-id', this.flag);

        return comboBox;
    }

    getSpinButton(min, max, setting)
    {
        let spinButton = new Gtk.SpinButton(this.widgetDefaults);
        spinButton.set_range(min, max);
        spinButton.set_increments(1, 2);
        this.settings.bind(setting, spinButton, 'value', this.flag);

        return spinButton;
    }

    getCheckButton(text, setting)
    {
        let checkButton = new Gtk.CheckButton({
            label: text || null,
        });
        this.settings.bind(setting, checkButton, 'active', this.flag);

        return checkButton;
    }

    _onClose(name)
    {
        if(name)
            debug(`cleanup of prefs ${name} page`);
    }
});
