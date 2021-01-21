const { Gio, GObject, Gtk } = imports.gi;
const Debug = imports.src.debug;
const Misc = imports.src.misc;

const { debug } = Debug;
const { settings } = Misc;

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
        const widget = (item.pages)
            ? new Notebook(item.pages, true)
            : new item.widget();

        this.addToNotebook(widget, item.title);
    }

    addToNotebook(widget, title)
    {
        const label = new Gtk.Label({
            label: title,
        });
        this.append_page(widget, label);
    }

    _onClose()
    {
        const totalPages = this.get_n_pages();
        let index = 0;

        while(index < totalPages) {
            const page = this.get_nth_page(index);
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
        const label = this.getLabel(text, true);

        return this.addToGrid(label);
    }

    addComboBoxText(text, entries, setting)
    {
        const label = this.getLabel(text + ':');
        const widget = this.getComboBoxText(entries, setting);

        return this.addToGrid(label, widget);
    }

    addSpinButton(text, min, max, setting, precision)
    {
        const label = this.getLabel(text + ':');
        const widget = this.getSpinButton(min, max, setting, precision);

        return this.addToGrid(label, widget);
    }

    addCheckButton(text, setting)
    {
        const widget = this.getCheckButton(text, setting);

        return this.addToGrid(widget);
    }

    addPlayFlagCheckButton(text, flag)
    {
        const checkButton = this.addCheckButton(text);
        const playFlags = settings.get_int('play-flags');

        checkButton.active = ((playFlags & flag) === flag);
        checkButton.connect('toggled', this._onPlayFlagToggled.bind(this, flag));

        return checkButton;
    }

    addFontButton(text, setting)
    {
        const label = this.getLabel(text + ':');
        const widget = this.getFontButton(setting);

        return this.addToGrid(label, widget);
    }

    getLabel(text, isTitle)
    {
        const marginTop = (isTitle && this.gridIndex > 0) ? 16 : 0;
        const marginBottom = (isTitle) ? 2 : 0;

        let marginLR = 0;

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
        const comboBox = new Gtk.ComboBoxText(this.widgetDefaults);

        for(let entry of entries)
            comboBox.append(entry[0], entry[1]);

        settings.bind(setting, comboBox, 'active-id', this.flag);

        return comboBox;
    }

    getSpinButton(min, max, setting, precision)
    {
        precision = precision || 1;

        const spinButton = new Gtk.SpinButton(this.widgetDefaults);
        spinButton.set_range(min, max);
        spinButton.set_digits(precision % 1 === 0 ? 0 : 3);
        spinButton.set_increments(precision, 1);
        settings.bind(setting, spinButton, 'value', this.flag);

        return spinButton;
    }

    getCheckButton(text, setting)
    {
        const checkButton = new Gtk.CheckButton({
            label: text || null,
        });

        if(setting)
            settings.bind(setting, checkButton, 'active', this.flag);

        return checkButton;
    }

    getFontButton(setting)
    {
        const fontButton = new Gtk.FontButton({
            use_font: true,
            use_size: true,
        });
        settings.bind(setting, fontButton, 'font', this.flag);

        return fontButton;
    }

    _onPlayFlagToggled(flag, button)
    {
        let playFlags = settings.get_int('play-flags');

        if(button.active)
            playFlags |= flag;
        else
            playFlags &= ~flag;

        settings.set_int('play-flags', playFlags);
    }

    _onClose(name)
    {
        if(name)
            debug(`cleanup of prefs ${name} page`);
    }
});
