#include "MySerialSocket.h"

void MySerialSocket::setRxMode(char rxMode) {
  if (rxMode == MSS_RXMODE_TEXT) {
    _rxMode = MSS_RXMODE_TEXT;
    MSSDEBUG("Set Rx mode text");
    _bcastRxMode();
  } else if (rxMode == MSS_RXMODE_BINARY) {
    _rxMode = MSS_RXMODE_BINARY;
    MSSDEBUG("Set Rx mode binary");
    _bcastRxMode();
  }
}

void MySerialSocket::setBaudRate(unsigned long newBaud) {
  _baudRate = newBaud;
  if (_brCallback != NULL) {
    _brCallback(newBaud);
  }
}

void MySerialSocket::_bcastBaudRate() {
  MSSDEBUG("Bcast baud rate");
  _ws->textAll(_bufferBaudRate());
}

void MySerialSocket::_bcastRxMode() {
  MSSDEBUG("Bcast rx mode");
  
  _ws->textAll(_bufferRxMode());
}

void MySerialSocket::_bcastRxText(const uint8_t* buffer, size_t size) {
  MSSDEBUG("Bcast rx text");
  // Append the "Rx" prefix, then send as text
  _bcastText(MSS_MSGPFX_RX, buffer, size);
}

void MySerialSocket::_bcastRxBinary(const uint8_t* buffer, size_t size) {
  MSSDEBUG("Bcast rx binary");
  // Just send that whole thing as binary
  _bcastBinary(MSS_MSGPFX_RX, buffer, size);
}

void MySerialSocket::_bcastTxText(const uint8_t* buffer, size_t size) {
  MSSDEBUG("Bcast tx text");
  // Append the "Rx" prefix, then send as text
  _bcastText(MSS_MSGPFX_TX, buffer, size);
}

void MySerialSocket::_bcastTxBinary(const uint8_t* buffer, size_t size) {
  MSSDEBUG("Bcast tx binary");
  // Append the "Rx" prefix, then send as text
  _bcastBinary(MSS_MSGPFX_TX, buffer, size);
}

void MySerialSocket::_bcastText(char prefix, const uint8_t* buffer, size_t size) {
  char _buffer[size+1];
  _buffer[0] = prefix;
  memcpy(_buffer+1, buffer, size+1);

  _ws->textAll((const char *)_buffer, size+1);
}

// TODO this feels like double handling
void MySerialSocket::_bcastBinary(char prefix, const uint8_t* buffer, size_t size) {
  char _buffer[size+1];
  _buffer[0] = prefix;
  memcpy(_buffer+1, buffer, size+1);

  _ws->binaryAll((const char *)_buffer, size+1);
}




#define MSS_ULONGBUFFERSIZE sizeof(unsigned long)*8+1+1 // size of ulong * 8 for bytes + 1 for null termination + 1 for MSS prefix
AsyncWebSocketMessageBuffer* MySerialSocket::_bufferBaudRate() {
  char baudStringBuffer [MSS_ULONGBUFFERSIZE];
  baudStringBuffer[0] = MSS_MSGPFX_BAUDRATE;
  ultoa(_baudRate, baudStringBuffer+1, 10);
  
  AsyncWebSocketMessageBuffer* awsBuffer = new AsyncWebSocketMessageBuffer((uint8_t*) baudStringBuffer, strlen(baudStringBuffer));
  return awsBuffer;
}

AsyncWebSocketMessageBuffer* MySerialSocket::_bufferRxMode() {
  uint8_t buffer[2];
  buffer[0] = MSS_MSGPFX_RXMODE;
  buffer[1] = _rxMode;
  
  AsyncWebSocketMessageBuffer* awsBuffer = new AsyncWebSocketMessageBuffer(buffer, 2);
  return awsBuffer;
}


// Setup
void MySerialSocket::begin(AsyncWebServer *server, const char * webUrl, const char* socketUrl) {
  _rxMode = MSS_RXMODE_TEXT;
  _server = server;
  _ws = new AsyncWebSocket(socketUrl);

  // Send the web page
  _server->on(webUrl, HTTP_GET, [](AsyncWebServerRequest *request){
      // Send Webpage
      AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", (const uint8_t*)MSS_WEB, strlen(MSS_WEB));
      //response->addHeader("Content-Encoding","gzip");
      request->send(response);        
  });

  _ws->onEvent([&](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    //WS_EVT_CONNECT, WS_EVT_DISCONNECT, WS_EVT_PONG, WS_EVT_ERROR, WS_EVT_DATA
    switch (type) {
      case WS_EVT_CONNECT:
        MSSDEBUG("Client connected");
        client->text("Hello");
        client->text(_bufferBaudRate());
        client->text(_bufferRxMode());
        break;
        
      case WS_EVT_DISCONNECT:
        MSSDEBUG("Client disconnected");
        break;
        
      case WS_EVT_PONG:
        MSSDEBUG("PONG");
        break;
        
      case WS_EVT_ERROR:
        MSSDEBUG("Socket error");
        // TODO debug error
        break;
        
      case WS_EVT_DATA:
        MSSDEBUG("Received data");
        // Echo
        if (len > 1) {
          char msgPrefix = data[0];

          switch (msgPrefix) {
            case MSS_MSGPFX_TX:
              // Do msg received handler
              if(_msgCallback != NULL){
                // Call the callback, minus the prefix
                _msgCallback(data+1, len-1);
              }

              // Echo to all clients
              //_ws->textAll(data, len);
              if (_rxMode == MSS_RXMODE_TEXT) {
                _ws->textAll(data, len);
              } else {
                _ws->binaryAll(data, len);
              }
              break;

            case MSS_MSGPFX_RXMODE:
              if (len >= 2) {
                setRxMode((char) data[1]);
              }
              break;

            case MSS_MSGPFX_BAUDRATE:
              char _baudBuffer[sizeof(unsigned long)*8+1]; // buffer big enough to fit a long
              memset(_baudBuffer, 0, sizeof(_baudBuffer));
              memcpy(_baudBuffer, data+1, min(sizeof(_baudBuffer), len-1));
    
              unsigned long newBaud = strtoul(_baudBuffer, NULL, 0);
              if (newBaud == 0) {
                // Error
                //_ws->text(client->id(), "Invalid baud rate");
                return;
              }

              setBaudRate(newBaud);
              MSSDEBUG("Baud rate set:");
              MSSDEBUG(_baudBuffer);
              
              // Echo baud rate change to clients
              _bcastBaudRate();
              break;
          }
        }
        
        break;
        
      default:
        MSSDEBUG("Unknown socket event "+type);
        break;
    }
  });

  // Add the web socket to the server.
  _server->addHandler(_ws);

  MSSDEBUG("Started");
}

// Set the "Message Received" callback function
void MySerialSocket::onMsgRecv(void (*msgCallback)(uint8_t *data, size_t len)) {
  _msgCallback = msgCallback;
}

void MySerialSocket::onBaudRateChange(void (*brCallback)(unsigned long baud)) {
  _brCallback = brCallback;
}

// Print class extension
// Print a single character
size_t MySerialSocket::write(uint8_t c) {
  if (_rxMode == MSS_RXMODE_TEXT) {
    _bcastRxText(&c, 1);
  } else if (_rxMode == MSS_RXMODE_BINARY) {
    _bcastRxBinary(&c, 1);
  }
  
  return(1);
}

// Print class extension
// Print a buffer of data
size_t MySerialSocket::write(const uint8_t* buffer, size_t size) {
  if (_rxMode == MSS_RXMODE_TEXT) {
    _bcastRxText((const uint8_t*)buffer, size);
  } else if (_rxMode == MSS_RXMODE_BINARY) {
    _bcastRxBinary((const uint8_t*)buffer, size);
  }
  
  return(size);
}


// Think
void MySerialSocket::cleanupClients() {
  if (_ws != NULL) {
    _ws->cleanupClients();
  }
}

// Debug function
void MySerialSocket::MSSDEBUG(const char *message){
  #if MSS_DEBUG
  Serial.print("[MSS] ");
  Serial.println(message);
  #endif
}
