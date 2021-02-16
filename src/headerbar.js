const { GObject, Gtk } = imports.gi;
const { HeaderBarBase } = imports.src.headerbarBase;

var HeaderBar = GObject.registerClass(
class ClapperHeaderBar extends HeaderBarBase
{
    _init()
    {
        super._init();
        this.add_css_class('osdheaderbar');
    }

    _onWindowButtonActivate(action)
    {
        this.activate_action(action, null);
    }

    _onFloatButtonClicked()
    {
        const clapperWidget = this.root.child;

        clapperWidget.controlsRevealer.toggleReveal();

        /* Reset timer to not disappear during click */
        clapperWidget.player._setHideControlsTimeout();
    }

    _onFullscreenButtonClicked()
    {
        this.root.fullscreen();
    }
});
