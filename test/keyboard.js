#!/usr/bin/gjs

imports.gi.versions.Gdk = '4.0';
imports.gi.versions.Gtk = '4.0';

const { Gdk, GObject, Gtk } = imports.gi;

var KeyboardTest = GObject.registerClass(
class ClapperKeyboardTest extends Gtk.Application
{
    _init(opts)
    {
        super._init({
            application_id: 'com.github.rafostar.ClapperKeyboardTest'
        });
    }

    vfunc_startup()
    {
        super.vfunc_startup();

        let window = new Gtk.ApplicationWindow({
            application: this,
            title: 'Clapper Keyboard Test',
        });

        let grid = new Gtk.Grid({
            margin_top: 10,
            margin_bottom: 10,
            margin_start: 20,
            margin_end: 20,
            row_spacing: 4,
            column_spacing: 8,
            focusable: true,
            can_focus: true,
            halign: Gtk.Align.CENTER,
            valign: Gtk.Align.CENTER,
        });

        let label;

        label = new Gtk.Label({ label: 'KEY:' });
        grid.attach(label, 0, 0, 1, 1);
        label = new Gtk.Label({ label: 'none' });
        grid.attach(label, 1, 0, 1, 1);

        label = new Gtk.Label({ label: 'VALUE:' });
        grid.attach(label, 0, 1, 1, 1);
        label = new Gtk.Label({ label: '0' });
        grid.attach(label, 1, 1, 1, 1);

        label = new Gtk.Label({ label: 'CODE:' });
        grid.attach(label, 0, 2, 1, 1);
        label = new Gtk.Label({ label: '0' });
        grid.attach(label, 1, 2, 1, 1);

        let keyController = new Gtk.EventControllerKey();
        keyController.connect('key-pressed', this._onKeyPressed.bind(this));
        grid.add_controller(keyController);

        window.set_child(grid);
    }

    vfunc_activate()
    {
        this.active_window.present();
        this.active_window.get_child().grab_focus();
    }

    _onKeyPressed(controller, keyval, keycode, state)
    {
         let grid = controller.get_widget();

         let keyName = grid.get_child_at(1, 0);
         keyName.set_label(Gdk.keyval_name(keyval));

         let keyVal = grid.get_child_at(1, 1);
         keyVal.set_label(String(keyval));

         let keyCode = grid.get_child_at(1, 2);
         keyCode.set_label(String(keycode));
    }
});

new KeyboardTest().run([]);
