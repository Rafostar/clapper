const { Gtk } = imports.gi;
const Dialogs = imports.src.dialogs;
const Prefs = imports.src.prefs;
const Misc = imports.src.misc;

var actions = {
    open_local: ['<Ctrl>O'],
    export_playlist: ['<Ctrl>E'],
    open_uri: ['<Ctrl>U'],
    prefs: null,
    shortcuts: ['F1', '<Ctrl>question'],
    about: null,
    progress_forward: ['Right'],
    progress_backward: ['Left'],
    next_chapter: ['<Shift>Right'],
    prev_chapter: ['<Shift>Left'],
    next_track: ['<Ctrl>Right'],
    prev_track: ['<Ctrl>Left'],
    volume_up: ['Up'],
    volume_down: ['Down'],
    mute: ['<Ctrl>M', 'M'],
    toggle_play: ['space'],
    change_repeat: ['<Ctrl>R'],
    reveal_controls: ['Return'],
    toggle_fullscreen: ['F11', 'F'],
    leave_fullscreen: ['Escape'],
    quit: ['<Ctrl>Q', 'Q'],
};

function handleAction(action, window)
{
    const clapperWidget = window.child;
    if(!clapperWidget) return;

    const { player } = clapperWidget;
    let bool = false;

    switch(action.name) {
        case 'open_local':
        case 'export_playlist':
            new Dialogs.FileChooser(window, action.name);
            break;
        case 'open_uri':
            new Dialogs.UriDialog(window);
            break;
        case 'prefs':
            new Prefs.PrefsWindow(window);
            break;
        case 'shortcuts':
            if(!window.get_help_overlay()) {
                const helpBuilder = Misc.getBuilderForName('help-overlay.ui');
                window.set_help_overlay(helpBuilder.get_object('help_overlay'));
            }
            clapperWidget.activate_action('win.show-help-overlay', null);
            break;
        case 'about':
            new Dialogs.AboutDialog(window);
            break;
        case 'progress_forward':
            bool = true;
        case 'progress_backward':
            player.adjust_position(bool);
            if(
                clapperWidget.isReleaseKeyEnabled
                && clapperWidget.isFullscreenMode
            )
                clapperWidget.revealControls();
            /* Actual seek is handled on release */
            clapperWidget.isReleaseKeyEnabled = true;
            if(!clapperWidget.has_focus)
                clapperWidget.grab_focus();
            break;
        case 'volume_up':
            bool = true;
        case 'volume_down':
            player.adjust_volume(bool);
            break;
        case 'mute':
            player.mute ^= true;
            break;
        case 'next_track':
            player.playlistWidget.nextTrack();
            break;
        case 'prev_track':
            player.playlistWidget.prevTrack();
            break;
        case 'reveal_controls':
            if(clapperWidget.isFullscreenMode)
                clapperWidget.revealControls();
            break;
        case 'leave_fullscreen':
            if(!clapperWidget.isFullscreenMode)
                break;
        case 'toggle_fullscreen':
            clapperWidget.toggleFullscreen();
            break;
        case 'change_repeat':
            player.playlistWidget.changeRepeatMode();
            break;
        case 'quit':
            clapperWidget.activate_action('window.close', null);
            break;
        case 'toggle_play':
        case 'next_chapter':
        case 'prev_chapter':
            player[action.name]();
            break;
        default:
            break;
    }
}
