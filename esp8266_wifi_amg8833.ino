// ESP8266 Pins
//  4(SDA) --- AMG8833 SDA
//  5(SCL) --- AMG8833 SCL
//  13     --- LED (Anode) via 100ohm

#include <pgmspace.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <Adafruit_AMG88xx.h>

Adafruit_AMG88xx amg;

const char* ssid = "HUAWEI-ZNJ";
const char* password = "ZNJketizu666";

const int pin_led = 16;
static const uint8_t pwm_A = 16 ;
static const uint8_t pwm_B = 16;
static const uint8_t dir_A = 16;
static const uint8_t dir_B = 16;
int motor_speed = 1024;
ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      break;
  }
}

void toggle() {
  static bool last_led = false;
  last_led = !last_led;
  digitalWrite(pin_led, last_led);
}

void handleRoot() {
  auto ip = WiFi.localIP();
  String ip_str = String(ip[0]) + "." + ip[1] + "." + ip[2] + "." + ip[3];
  server.send(200, "text/html", String(ws_html_1()) +  ip_str + ws_html_2());
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void WiFiEvent(WiFiEvent_t event) {
    switch(event) {      
      case WIFI_EVENT_STAMODE_DISCONNECTED:
        digitalWrite(pin_led, LOW);
        Serial.println("WiFi lost connection: reconnecting...");
        WiFi.begin();
        break;
      case WIFI_EVENT_STAMODE_CONNECTED:
        Serial.print("Connected to ");
        Serial.println(ssid);
        break;
      case WIFI_EVENT_STAMODE_GOT_IP:
        digitalWrite(pin_led, HIGH);
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        if (MDNS.begin("esp8266-amg8833")) {
          Serial.println("MDNS responder started");
        }
        enableOTA();
        break;
    }
}

void enableOTA() {
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

String webPage = "";

void setup(void) {
  pinMode(pin_led, OUTPUT);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(WiFiEvent);
  WiFi.begin(ssid, password);
  Serial.println("");

  server.on("/", handleRoot);

  server.on("/current", [](){
    String str;
    server.send(200, "text/plain", get_current_values_str(str));
  });

  server.onNotFound(handleNotFound);

  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Serial.println("HTTP server started");
Wire.begin(2,14);
  amg.begin();
  delay(100); // let sensor boot up
}

void loop(void) {
  ArduinoOTA.handle();
  server.handleClient();
  webSocket.loop();

  // Wait for connection
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long last_ms;
    unsigned long t = millis();
    if (t - last_ms > 500) {
      Serial.print(".");
      toggle();
      last_ms = t;
    }
  } else {
    digitalWrite(pin_led, millis() % 3000 < 200);
  }

  static unsigned long last_read_ms = millis();
  unsigned long now = millis();
  if (now - last_read_ms > 100) {
    last_read_ms += 100;
    String str;
    get_current_values_str(str);
   // Serial.println(str);
    webSocket.broadcastTXT(str);
  }
}

String& get_current_values_str(String& ret)
{
  float pixels[AMG88xx_PIXEL_ARRAY_SIZE];
  amg.readPixels(pixels);
  ret = "[";
  for(int i = 0; i < AMG88xx_PIXEL_ARRAY_SIZE; i++) {
    if( i % 8 == 0 ) ret += "\r\n";
    ret += pixels[i];
    if (i != AMG88xx_PIXEL_ARRAY_SIZE - 1) ret += ", ";
  }
  ret += "\r\n]\r\n";
  return ret;
}

const __FlashStringHelper* ws_html_1() {
  return F("<!DOCTYPE html>\n"
    "<html>\n"
    "<head>\n"
    "<title>Thermo</title>\n"
    "<style>\n"
    "body {\n"
    "    background-color: #667;\n"
    "}\n"
    "table#tbl td {\n"
    "    width: 135px;\n"
    "    height: 135px;\n"
    "    border: solid 1px grey;\n"
    "    text-align: center;\n"
    "}\n"
    "</style>\n"
    "</head>\n"
    "<body>\n"
    "<table border id=\"tbl\"></table>\n"
    "<script>\n"
    "function bgcolor(t) {\n"
    "    if (t < 0) t = 0;\n"
    "    if (t > 30) t = 30;\n"
    "    return \"hsl(\" + (360 - t * 12) + \", 100%, 80%)\";\n"
    "}\n"
    "\n"
    "var t = document.getElementById('tbl');\n"
    "var tds = [];\n"
    "for (var i = 0; i < 8; i++) {\n"
    "    var tr = document.createElement('tr');\n"
    "    for (var j = 0; j < 8; j++) {\n"
    "        var td = tds[i*8 + 7 - j] = document.createElement('td');\n"
    "        tr.appendChild(td);\n"
    "    }\n"
    "    t.appendChild(tr);\n"
    "}\n"
    

    "var connection = new WebSocket('ws://");
    
}                 

const __FlashStringHelper* ws_html_2() {
  return F(":81/');\n"
    "connection.onmessage = function(e) {\n"
    "    const data = JSON.parse(e.data);\n"
    "    for (var i = 0; i < 64; i++) {\n"
    "        tds[i].innerHTML = data[i].toFixed(2);\n"
    "        tds[i].style.backgroundColor = bgcolor(data[i]);\n"
    "    }\n"
    "};\n"
    "</script>\n"
    "<button type=\"button\">Click Me!</button>"
    "</body>\n"
    "</html>\n");
}

void FL() {
  digitalWrite(16, HIGH);
  Serial.println("ok");
}
