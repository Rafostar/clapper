const { GObject } = imports.gi;
const { HeaderBarBase } = imports.src.headerbarBase;

var HeaderBarRemote = GObject.registerClass({
    GTypeName: 'ClapperHeaderBarRemote',
},
class ClapperHeaderBarRemote extends HeaderBarBase
{
    _init()
    {
        super._init();
        this.extraButtonsBox.visible = false;
    }

    _onWindowButtonActivate(action)
    {
        if(action === 'toggle-maximized')
            action = 'toggle_maximized';

        this.root.child.sendWs(action);
    }
});
