#define DEBUG 0

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
  #include <ESP8266mDNS.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <AsyncTCP.h>
  #include <ESPmDNS.h>
#endif
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#include "MySerialSocket.h";

#define HOSTNAME "ESP8266-" ///< Hostename. The setup function adds the Chip ID at the end.

AsyncWebServer server(80);
MySerialSocket mss;

const char* wifiSsid = "The Interwebs"; 
const char* wifiPass = "ghostgummybear";

// Set baud rate callback
void cbSetBaudRate(unsigned long newBaud) {
  Serial.begin(newBaud);
}

// Incoming message callback
void cbRecvMsg(uint8_t *data, size_t len) {
  for (size_t i=0; i < len; i++) {
    Serial.print((char) data[i]); // Convert to char, otherwise it prints the ASCII numbers of the text sent
  }
}

#define SERLISTEN_BUFFER_SIZE 128 // 128 bytes. Most Arduino UART buffer is 64 bytes.
#define SERLISTEN_BUFFER_WAIT 100 // How often to collect serial data for
unsigned long serlistenStarted = 0;
uint8_t serlistenBuffer[SERLISTEN_BUFFER_SIZE];
uint8_t serlistenBufferIdx = 0;

// Listen for incoming data from serial.
// If there's data, start listening and buffering.
// Wait until either the listen time limit or the buffer is full,
// then send it to clients via the SerialSocket.
void loop_serialToSocket() {

  // If there's data available AND the buffer is empty
  // remember when we started buffering.
  if (Serial.available() && serlistenBufferIdx == 0) {
    serlistenStarted = millis();
  }
  
  // Buffer the incoming data
  while (Serial.available() && serlistenBufferIdx < SERLISTEN_BUFFER_SIZE) {
    serlistenBuffer[serlistenBufferIdx] = Serial.read();
    serlistenBufferIdx++;
  }

  // Did we hit the buffer limit or the time limit?
  // If so, send what's in the buffer and reset.
  if (serlistenBufferIdx > 0 && (serlistenBufferIdx >= SERLISTEN_BUFFER_SIZE || millis() - serlistenStarted > SERLISTEN_BUFFER_WAIT ) ) {
    mss.write(serlistenBuffer, serlistenBufferIdx); // Send what's in the buffer
    serlistenBufferIdx = 0; // Reset the buffer
  }
}

#define MSS_CLEANUP_INTERVAL 10000
unsigned long mssLastCleanup = 0;

//==================
// MAIN
//==================
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  #if DEBUG
  Serial.println("Starting");
  #endif

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid, wifiPass);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.printf("WiFi Failed!\n");
      return;
  }

  #if DEBUG
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  #endif

  //if (!MDNS.begin(HOSTNAME+'-'+String(ESP.getChipId(), HEX))) {
  String _hostname = String(HOSTNAME)+String(ESP.getChipId(), HEX);
  if (!MDNS.begin(_hostname)) {
    #if DEBUG
    Serial.print("mDNS setup failed");
    #endif
  } else {
    #if DEBUG
    Serial.print("Host "); Serial.println(_hostname);
    #endif
  }
  
  mss.begin(&server);
  mss.setBaudRate(115200);
  mss.onMsgRecv(&cbRecvMsg);
  mss.onBaudRateChange(&cbSetBaudRate);
  
  AsyncElegantOTA.begin(&server);

  // Start the server
  server.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  if (millis() - mssLastCleanup >= MSS_CLEANUP_INTERVAL) {
    mss.cleanupClients();
    mssLastCleanup = millis();
  }
  loop_serialToSocket();
}
