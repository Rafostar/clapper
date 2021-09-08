const { GObject } = imports.gi;
const { HeaderBarBase } = imports.headerbarBase;

var HeaderBar = GObject.registerClass({
    GTypeName: 'ClapperHeaderBar',
},
class ClapperHeaderBar extends HeaderBarBase
{
    _onWindowButtonActivate(action)
    {
        this.activate_action(`window.${action}`, null);
    }

    _onFloatButtonClicked(button)
    {
        const clapperWidget = this.root.child;
        const { controlsRevealer } = clapperWidget;

        controlsRevealer.toggleReveal();

        /* Reset timer to not disappear during click */
        clapperWidget._setHideControlsTimeout();

        this._updateFloatIcon(!controlsRevealer.reveal_child);
    }

    _onFullscreenButtonClicked(button)
    {
        this.root.fullscreen();
    }
});
