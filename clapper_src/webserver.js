const { Soup, GObject } = imports.gi;
const ByteArray = imports.byteArray;
const Debug = imports.clapper_src.debug;

let { debug } = Debug;

var WebServer = GObject.registerClass(
class ClapperWebServer extends Soup.Server
{
    _init(port)
    {
        super._init();

        this.isListening = false;
        this.listeningPort = null;
        this.wsConns = [];

        if(port)
            this.setListeningPort(port);
    }

    setListeningPort(port)
    {
        if(!port)
            return;

        const wasListening = this.isListening;

        if(wasListening)
            this.stopListening();

        this.listeningPort = port;

        if(wasListening)
            this.startListening();
    }

    startListening()
    {
        if(this.isListening || !this.listeningPort)
            return;

        let isListening = false;

        this.add_handler('/', this._onDefaultAccess.bind(this));
        this.add_websocket_handler('/websocket', null, null, this._onWsConnection.bind(this));

        try {
            isListening = this.listen_local(this.listeningPort, Soup.ServerListenOptions.IPV4_ONLY);
        }
        catch(err) {
            debug(err);
        }

        if(isListening) {
            const uris = this.get_uris();
            const usedPort = uris[0].get_port();
            debug(`WebSocket server listening on port: ${usedPort}`);
        }
        else {
            debug(new Error('WebSocket server could not start listening'));
            this._closeCleanup();
        }

        this.isListening = isListening;
    }

    stopListening()
    {
        if(!this.isListening)
            return;

        this._closeCleanup();
        this.disconnect();

        this.isListening = false;
        this.listeningPort = null;
    }

    sendMessage(data)
    {
        for(const connection of this.wsConns) {
            if(connection.state !== Soup.WebsocketState.OPEN)
                continue;

            connection.send_text(JSON.stringify(data));
        }
    }

    passMsgData(action, value)
    {
    }

    _closeCleanup()
    {
        while(this.wsConns.length) {
            const connection = this.wsConns.pop();

            if(connection.state !== Soup.WebsocketState.OPEN)
                continue;

            connection.close(Soup.WebsocketCloseCode.NORMAL, null);
        }

        this.remove_handler('/websocket');
        this.remove_handler('/');
    }

    _onWsConnection(server, connection)
    {
        debug('new WebSocket connection');

        connection.connect('message', this._onWsMessage.bind(this));
        connection.connect('closed', this._onWsClosed.bind(this));

        this.wsConns.push(connection);
        debug(`total WebSocket connections: ${this.wsConns.length}`);
    }

    _onWsMessage(connection, dataType, bytes)
    {
        if(dataType !== Soup.WebsocketDataType.TEXT)
            return debug('ignoring non-text WebSocket message');

        const msg = bytes.get_data();
        let parsedMsg = null;

        try {
            parsedMsg = JSON.parse(ByteArray.toString(msg));
        }
        catch(err) {
            debug(err);
        }

        if(!parsedMsg || !parsedMsg.action)
            return debug('no "action" in parsed WebSocket message');

        this.passMsgData(parsedMsg.action, parsedMsg.value || 0);
    }

    _onWsClosed(connection)
    {
        debug('closed WebSocket connection');

        this.wsConns = this.wsConns.filter(conn => conn !== connection);
        debug(`remaining WebSocket connections: ${this.wsConns.length}`);
    }

    _onDefaultAccess(server, msg)
    {
        msg.status_code = 404;
    }
});
