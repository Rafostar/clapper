const { GObject, Gtk } = imports.gi;
const { HeaderBarBase } = imports.src.headerbarBase;

var HeaderBar = GObject.registerClass(
class ClapperHeaderBar extends HeaderBarBase
{
    _init(window)
    {
        super._init(window);

        this.title_widget.visible = false;
    }

    _onFloatButtonClicked()
    {
        const clapperWidget = this.root.child;

        clapperWidget.controlsRevealer.toggleReveal();
    }

    _onFullscreenButtonClicked()
    {
        this.root.fullscreen();
    }
});
