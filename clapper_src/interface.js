const { GObject, Gtk, GstPlayer } = imports.gi;
const { Controls } = imports.clapper_src.controls;

var Interface = GObject.registerClass(
class ClapperInterface extends Gtk.Grid
{
    _init()
    {
        super._init();

        this.lastVolumeValue = null;
        this.lastPositionValue = 0;

        this.controls = new Controls();
        this.attach(this.controls, 0, 1, 1, 1);
    }

    addPlayer(player)
    {
        this._player = player;
        this._player.widget.expand = true;

        this._player.connect('state-changed', this._onPlayerStateChanged.bind(this));
        this._player.connect('volume-changed', this._onPlayerVolumeChanged.bind(this));
        this._player.connect('duration-changed', this._onPlayerDurationChanged.bind(this));
        this._player.connect('position-updated', this._onPlayerPositionUpdated.bind(this));

        this.controls.togglePlayButton.connect(
            'clicked', this._onControlsTogglePlayClicked.bind(this)
        );
        this.controls.positionScale.connect(
            'value-changed', this._onControlsPositionChanged.bind(this)
        );
        this.controls.volumeButton.connect(
            'value-changed', this._onControlsVolumeChanged.bind(this)
        );

        this.attach(this._player.widget, 0, 0, 1, 1);
    }

    _onPlayerStateChanged(player, state)
    {
        switch(state) {
            case GstPlayer.PlayerState.STOPPED:
                break;
            case GstPlayer.PlayerState.BUFFERING:
                break;
            case GstPlayer.PlayerState.PAUSED:
                this.controls.togglePlayButton.image = this.controls.playImage;
                break;
            case GstPlayer.PlayerState.PLAYING:
                this.controls.togglePlayButton.image = this.controls.pauseImage;
                break;
            default:
                break;
        }
    }

    _onPlayerDurationChanged(player)
    {
        let duration = player.get_duration() / 1000000000;
        let increment = (duration < 1)
            ? 0
            : (duration < 100)
            ? 1
            : duration / 100;

        this.controls.positionAdjustment.set_upper(duration);
        this.controls.positionAdjustment.set_step_increment(increment);
        this.controls.positionAdjustment.set_page_increment(increment);
    }

    _onPlayerPositionUpdated(player, position)
    {
        let positionSeconds = position / 1000000000;
        let positionFloor = Math.floor(positionSeconds);

        if(positionFloor === this.lastPositionValue)
            return;

        this.lastPositionValue = positionFloor;
        this.controls.positionScale.set_value(positionSeconds);
    }

    _onPlayerVolumeChanged()
    {
        let volume = Number(this._player.get_volume().toFixed(2));

        if(volume === this.lastVolumeValue)
            return;

        this.controls.volumeButton.set_value(volume);
    }

    _onControlsTogglePlayClicked()
    {
        this._player.toggle_play();
    }

    _onControlsPositionChanged(range)
    {
        let position = Math.floor(range.get_value());

        if(position === this.lastPositionValue)
            return;

        this.lastPositionValue = position;
        this._player.seek_seconds(position);
    }

    _onControlsVolumeChanged(widget, value)
    {
        if(value === this.lastVolumeValue)
            return;

        this.lastVolumeValue = value;
        this._player.set_volume(value);
    }
});
