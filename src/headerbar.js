const { GObject } = imports.gi;
const { HeaderBarBase } = imports.src.headerbarBase;

var HeaderBar = GObject.registerClass(
class ClapperHeaderBar extends HeaderBarBase
{
    _onWindowButtonActivate(action)
    {
        this.activate_action(`window.${action}`, null);
    }

    _onFloatButtonClicked()
    {
        const clapperWidget = this.root.child;

        clapperWidget.controlsRevealer.toggleReveal();

        /* Reset timer to not disappear during click */
        clapperWidget._setHideControlsTimeout();
    }

    _onFullscreenButtonClicked()
    {
        this.root.fullscreen();
    }
});
