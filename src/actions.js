const Dialogs = imports.src.dialogs;

var actions = {
    open_local: ['<Ctrl>O'],
    open_uri: ['<Ctrl>U'],
    prefs: null,
    about: null,
    progress_forward: ['Right'],
    progress_backward: ['Left'],
    next_chapter: ['<Shift>Right'],
    prev_chapter: ['<Shift>Left'],
    next_track: ['<Ctrl>Right'],
    prev_track: ['<Ctrl>Left'],
    volume_up: ['Up'],
    volume_down: ['Down'],
    toggle_play: ['space'],
    reveal_controls: ['Return'],
    toggle_fullscreen: ['F11', 'f'],
    quit: ['<Ctrl>q', 'q'],
};

function handleAction(action, window)
{
    const clapperWidget = window.child;
    if(!clapperWidget) return;

    const { player } = clapperWidget;
    let bool = false;

    switch(action.name) {
        case 'open_local':
            new Dialogs.FileChooser(window);
            break;
        case 'open_uri':
            new Dialogs.UriDialog(window);
            break;
        case 'prefs':
            new Dialogs.PrefsDialog(window);
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
        case 'toggle_fullscreen':
            clapperWidget.toggleFullscreen();
            break;
        case 'quit':
            clapperWidget.root.emit('close-request');
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
