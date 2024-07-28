#define MSS_DEBUG 0

#include "Print.h"
#if defined(ESP8266)
    #include "ESP8266WiFi.h"
    #include "ESPAsyncTCP.h"
    #include "ESPAsyncWebServer.h"
#elif defined(ESP32)
    #include "WiFi.h"
    #include "AsyncTCP.h"
    #include "ESPAsyncWebServer.h"
#endif

const char MSS_MSGPFX_TX = 't';
const char MSS_MSGPFX_RX = 'r';
const char MSS_MSGPFX_BAUDRATE = 'b';
const char MSS_MSGPFX_RXMODE = 'x';

const char MSS_RXMODE_TEXT = 't';
const char MSS_RXMODE_BINARY = 'b';

const char MSS_WEB[] = {R"==(
<html>
<head>
<title>MySerialSocket</title>
</head>
<body onload="_onLoad()">
<script>
function _onLoad() {
  var elem = document.getElementById("ui-link");
  if (elem) {
    elem.setAttribute("href", (elem.getAttribute("href") || "")+ "?c=" + location.hostname);
  }
}
</script>
<h1>MySerialSocket</h1>
<p><a id="ui-link" href="https://towerofpower256.github.io/MySerialSocket/ui">Open the UI</a></p>
</body>
</html>
)=="};

class MySerialSocket : public Print {
  public:
    void begin(AsyncWebServer *server, const char * webUrl = "/", const char* socketUrl = "/ws");

    void onMsgRecv(void (*msgCallback)(uint8_t *data, size_t len));
    void onBaudRateChange(void (*brCallback)(unsigned long baud));
    void cleanupClients();
    void setRxMode(char rxMode);
    void setBaudRate(unsigned long newBaud);

    // Print class support
    size_t write(uint8_t c);
    size_t write(const uint8_t* buffer, size_t size);

  private:
    AsyncWebServer *_server;
    AsyncWebSocket *_ws;
    char _rxMode;
    unsigned long _baudRate;

    void _bcastBaudRate();
    void _bcastRxMode();
    void _bcastRxText(const uint8_t* buffer, size_t size);
    void _bcastRxBinary(const uint8_t* buffer, size_t size);
    void _bcastTxText(const uint8_t* buffer, size_t size);
    void _bcastTxBinary(const uint8_t* buffer, size_t size);
    void _bcastText(char prefix, const uint8_t* buffer, size_t size);
    void _bcastBinary(char prefix, const uint8_t* buffer, size_t size);
    AsyncWebSocketMessageBuffer* _bufferBaudRate();
    AsyncWebSocketMessageBuffer* _bufferRxMode();
    void (*_msgCallback)(uint8_t *data, size_t len);
    void (*_brCallback)(unsigned long baud);
    
    void MSSDEBUG(const char *message);
};
