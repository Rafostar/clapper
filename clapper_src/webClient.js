const { Gio, GObject, Soup } = imports.gi;
const Debug = imports.clapper_src.debug;
const Misc = imports.clapper_src.misc;
const WebHelpers = imports.clapper_src.webHelpers;

let { debug } = Debug;
let { settings } = Misc;

var WebClient = GObject.registerClass(
class ClapperWebClient extends Soup.Session
{
    _init(port)
    {
        super._init({
            timeout: 3,
            use_thread_context: true,
        });

        this.wsConn = null;

        this.connectWebsocket();
    }

    connectWebsocket()
    {
        if(this.wsConn)
            return;

        const port = settings.get_int('webserver-port');
        const message = Soup.Message.new('GET', `ws://127.0.0.1:${port}/websocket`);
        this.websocket_connect_async(message, null, null, null, this._onWsConnect.bind(this));

        debug('connecting WebSocket to Clapper app');
    }

    sendMessage(data)
    {
        if(
            !this.wsConn
            || this.wsConn.state !== Soup.WebsocketState.OPEN
        )
            return;

        this.wsConn.send_text(JSON.stringify(data));
    }

    passMsgData(action, value)
    {
    }

    _onWsConnect(session, result)
    {
        let connection = null;

        try {
            connection = this.websocket_connect_finish(result);
        }
        catch(err) {
            debug(err);
        }

        if(!connection)
            return this.passMsgData('close');

        connection.connect('message', this._onWsMessage.bind(this));
        connection.connect('closed', this._onWsClosed.bind(this));

        this.wsConn = connection;

        debug('successfully connected WebSocket');
    }

    _onWsMessage(connection, dataType, bytes)
    {
        const [success, parsedMsg] = WebHelpers.parseData(dataType, bytes);

        if(success)
            this.passMsgData(parsedMsg.action, parsedMsg.value);
    }

    _onWsClosed(connection)
    {
        debug('closed WebSocket connection');
        this.wsConn = null;

        this.passMsgData('close');
    }
});
