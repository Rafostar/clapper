const { GObject } = imports.gi;
const { HeaderBarBase } = imports.src.headerbarBase;

var HeaderBar = GObject.registerClass(
class ClapperHeaderBar extends HeaderBarBase
{
    _init(window)
    {
        super._init(window);

        const clapperWidget = window.get_child();
        clapperWidget.controls.unfloatButton.bind_property('visible', this, 'visible',
            GObject.BindingFlags.INVERT_BOOLEAN
        );
    }

    _onFloatButtonClicked()
    {
        const clapperWidget = this.get_prev_sibling();
        clapperWidget.setFloatingMode(true);
    }

    _onFullscreenButtonClicked()
    {
        const window = this.get_parent();
        window.fullscreen();
    }
});
