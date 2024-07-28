var SOCK_STATE_CONNECTING = 0;
var SOCK_STATE_OPEN = 1;
var SOCK_STATE_CLOSING = 2;
var SOCK_STATE_CLOSED = 3;

var MSS_RXMODE_TEXT = 't';
var MSS_RXMODE_BINARY = 'b';

var MSS_TXMODE_TEXT = "t";
var MSS_TXMODE_BINARY = "b";

var MSS_LE_NONE = "none";
var MSS_LE_N = "n";
var MSS_LE_R = "r";
var MSS_LE_NR = "nr";

var MSS_MSGPFX_RX = 'r';
var MSS_MSGPFX_TX = 't';
var MSS_MSGPFX_BAUDRATE = 'b';
var MSS_MSGPFX_RXMODE = 'x';

class MySerialSocket {
    constructor() {
        this.hasError = false;
        this.error = undefined;
        this.socket = undefined;
        this.txMode = MSS_TXMODE_TEXT;
        this.lineEndMode = MSS_LE_N;
        this.lastMsgType = undefined;
    }

    setState(state) {
        console.log("SetState", state);
        this.state = state;

        this.doStateChange();
    }

    onStateChange(fn) {
        this.stateChangeCb = fn;
    }

    doStateChange() {
        if (typeof this.stateChangeCb == "function") this.stateChangeCb(this.state);
    }

    setLineEndMode(newMode) {
        this.lineEndMode = newMode;
    }

    onLineEndModeChange(fn) {
        this.lineEndModeCb = fn;
    }

    doLineEndModeChange(newMode) {
        if (typeof this.lineEndModeCb == "function") this.lineEndModeCb(this.lineEndMode);
    }

    setTxModeText() { this.setRxMode(MSS_TXMODE_TEXT); }
    setTxModeBinary() { this.setRxMode(MSS_TXMODE_BINARY); }
    setTxMode(a, silent = false) {
        this.txMode = a || MSS_TXMODE_TEXT;
        this.doTxModeChange(a);
        if (!silent) this.printInfo("Tx mode set: " + a);
    }

    onTxModeChange(fn) {
        this.txModeChangeCb = fn;
    }

    doTxModeChange() {
        if (typeof this.txModeChangeCb == "function") this.txModeChangeCb(this.txMode);
    }

    onPrint(fn) {
        this.printCb = fn;
    }

    doPrint(data) {
        if (typeof this.printCb == "function") this.printCb(data);
    }

    setRxModeText() { this.setRxMode(MSS_RXMODE_TEXT); }
    setRxModeBinary() { this.setRxMode(MSS_RXMODE_BINARY); }
    setRxMode(newMode) {
        var _newMode = String(newMode).substring(0, 1);
        this.socket.send(MSS_MSGPFX_RXMODE + _newMode);
    }

    setBaudRate(newBaud) {
        this.socket.send(MSS_MSGPFX_BAUDRATE + String(newBaud));
    }

    onBaudRateChange(fn) {
        this.baudRateChangeCb = fn;
    }

    doBaudRateChange(newBaud) {
        if (typeof this.baudRateChangeCb == "function") this.baudRateChangeCb(newBaud);
    }

    onConnect(fn) {
        this.connectCb = fn;
    }

    doConnect() {
        if (typeof this.connectCb == "function") this.baudRateChangeCb();
    }

    begin() {
        console.log("Starting");

        this.setState("disconnected");
    }

    setError(err) {
        this.hasError = true;
        this.error = err;

        this.println("ERROR");
        this.printInfo(err);
    }

    clearError() {
        this.hasError = false;
        this.error = undefined;
    }

    connect(url) {
        if (this.state != "disconnected") {
            throw ("Tried to connect but already connecting / connected");
        }

        if (!url || url == "") {
            console.log("Can't connect: empty URL");
            return;
        }

        try {
            this.setState("connecting");
            this.printInfo("Connecting: "+url);
            this.clearError();
            this.socket = new WebSocket(url, "myprotocol");
            this.socket.binaryType = "arraybuffer";

            this.socket.onopen = (ev) => {
                console.log("onopen", ev);

                this.setState("connected");
                this.printInfo("CONNECTED");
            };

            this.socket.onclose = (ev) => {
                console.log("onclose", ev);

                this.setState("disconnected");
                this.printInfo("DISCONNECTED");
            };

            this.socket.onerror = (ev) => {
                console.error("onerror", ev);
                this.printInfo("SOCKET ERROR");
            };

            this.socket.onmessage = (ev) => {
                console.log("Message", ev);

                // Text
                // Read it, looking for the prefix
                if (typeof ev.data == "string" && ev.data.length > 0) {
                    var prefix = ev.data[0];
                    if (prefix == MSS_MSGPFX_RX) {
                        this.printRx(ev.data.substr(1));
                    }
                    if (prefix == MSS_MSGPFX_TX) {
                        this.printTx(ev.data.substr(1));
                    }
                    if (prefix == MSS_MSGPFX_RXMODE) {
                        var newMode = ev.data.substr(1, 1);
                        this.printInfo("New Rx mode: " + newMode);
                        this.setTxMode(newMode);
                    }
                    if (prefix == MSS_MSGPFX_BAUDRATE) {
                        var newBaud = ev.data.substring(1);
                        this.printInfo("Baud rate set: " + newBaud);
                        this.doBaudRateChange(newBaud);
                    }
                }

                // Binary
                // Convert to HEX and dump to the terminal
                if (ev.data instanceof ArrayBuffer) {
                    // Check the prefix
                    var dataArr = new Uint8Array(ev.data);
                    var prefix = dataArr[0];
                    if (prefix == MSS_MSGPFX_RX.charCodeAt(0)) {
                        var dataAsHex = this.convertBytesToHex(ev.data, 1);
                        // TODO print prefix
                        this.printRx(dataAsHex, true);
                    } else if (prefix == MSS_MSGPFX_TX.charCodeAt(0)) {
                        // TODO print prefix
                        var dataAsHex = this.convertBytesToHex(ev.data, 1);
                        this.printTx(dataAsHex, true);
                    } else {
                        throw ("Unknown prefix in binary message");
                    }
                }
            };
        }
        catch (err) {
            this.state = "disconnected";
            this.setError(err);
            console.error(err);

            return;
        }

    }

    dispose() {
        this.disconnect();
    }

    // Function to convert HEX bytes into numerical bytes.
    // Must be groups of 2 hex digits each, worth a byte.
    // Can have spaces between bytes.
    // Examples
    // A2
    // A2 C5 42
    // A2C542
    // a2C542
    convertHexToBytes(a, prefixChar = "") {
        var formatTest = /^(([a-fA-F0-9]){2}( )*)+$/.test(a);
        if (!formatTest) {
            throw "Invalid hex byte format, must be a string of byte-pairs of hexadecimal digits";
        }

        // Remove all space characters
        a = a.replace(/ /g, "");

        // Build the array
        var arr = [prefixChar.charCodeAt(0)];
        var i = 0;
        while (i < a.length) {
            var slice = a.substr(i, 2);
            // Did we get 2 character, or was it short? It shouldn't be, but check anyway
            if (slice.length == 2) {

            }

            var byte = parseInt(slice, 16);
            if (isNaN(byte)) {
                throw "Value is not a hex byte-pair: " + slice;
            }

            arr.push(byte);

            i += 2;
        }

        var r = new Uint8Array(arr);
        return r;
    }

    // Convert a payload of bytes into a string of hex byte-pairs.

    convertBytesToHex(buffer, startIdx = 0) {
        if (!(buffer instanceof ArrayBuffer)) {
            throw ("Can only convert a Uint8Array to a hex string");
        }

        var arr = new Uint8Array(buffer);

        var r = [];
        for (var i = startIdx; i < arr.length; i++) {
            r.push(arr[i].toString(16).padStart(2, '0').toUpperCase());
        }

        return r.join(" ") + " ";
    }

    disconnect() {
        if (this.socket) {
            this.socket.close();
            delete this.socket;
        }
    }

    print(data = "") {
        this.doPrint(data);
    }

    println(data) {
        this.print(data);
        this.print("\n");
    }

    printInfo(data) {
        this.lastMsgType = undefined;
        this.printInfoPrefix();
        this.println(data);
    }

    printInfoPrefix() {
        this.print("# ");
    }

    printTx(data, forcePrefix) {
        if (forcePrefix || this.lastMsgType != MSS_MSGPFX_TX) {
            this.println();
            this.printTxPrefix();
            this.lastMsgType = MSS_MSGPFX_TX;
        }
        this.print(data);
    }

    printRx(data, forcePrefix) {
        if (forcePrefix || this.lastMsgType != MSS_MSGPFX_RX) {
            this.println();
            this.printRxPrefix();
            this.lastMsgType = MSS_MSGPFX_RX;
        }
        this.print(data);
    }

    printTxPrefix() {
        this.print("< ");
    }

    printRxPrefix() {
        this.print("> ");
    }

    send(data) {
        if (!data || data == "") return;

        try {


            if (this.socket && this.state == "connected") {
                if (this.txMode == MSS_TXMODE_BINARY) {
                    var binPayload = this.convertHexToBytes(data, MSS_MSGPFX_TX);
                    this.socket.send(binPayload);
                } else {
                    var _data = MSS_MSGPFX_TX + data;

                    switch (this.lineEndMode) {
                        case MSS_LE_NONE:
                            break;
                        case MSS_LE_N:
                            _data = _data + "\n";
                            break;
                        case MSS_LE_R:
                            _data = _data + "\r";
                            break;
                        case MSS_LE_NR:
                            _data = _data + "\n\r";
                            break;
                    }

                    this.socket.send(_data);
                }

                //this.terminalInputElement.value = "";
                return true;
            }
        } catch (err) {
            this.printInfo("ERROR: " + err);
            throw err;
        }
    }

    clearTerminal() {
        if (this.terminalOutputElement) {
            this.terminalOutputElement.value = "";
        }
    }

    
}



var mss = new MySocketSerial();