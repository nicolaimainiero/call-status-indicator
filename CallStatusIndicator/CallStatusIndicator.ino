#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <Adafruit_NeoPixel.h>
#include "Credentials.h"

#define CLIENT_ID "<client_id>"
#define TENANT "<tenant>"
#define WIFI_PASSWORD "<wifi-password>"
#define SSID "<wifi-ssid>"

ESP8266WebServer server(80);
HTTPClient https;
std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);

#define STARTING 0
#define NOT_AUTHENICATED 1
#define REFRESH_TOKEN 2
#define DEVICE_TOKEN_REQUEST 3
#define AUTHENICATED 4

#define PIN D7 // Hier wird angegeben, an welchem digitalen Pin die WS2812 LEDs bzw. NeoPixel angeschlossen sind
#define NUMPIXELS 60 // Hier wird die Anzahl der angeschlossenen WS2812 LEDs bzw. NeoPixel angegeben

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

volatile int current_state = 0;
char device_code[207];
DynamicJsonDocument token(6144);

bool debug = false;

void setup() {
  Serial.begin(115200);
  Serial.println();
  setup_filesystem();
  setup_wifi();
  setup_webserver();

  client->setInsecure();
  pixels.begin();
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(SSID);

  WiFi.begin(SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) //If WiFi connected to hot spot then start mDNS
  {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    if (MDNS.begin("call-status-indicator"))
    { //Start mDNS with name esp826
      Serial.println("mDNS started: call-status-indicator.local");
    }
  }
}

void setup_webserver() {
  Serial.println("Starting Webserver setup.");
  server.serveStatic("/static/", SPIFFS, "/static/");
  server.on("/", handleRootPath);
  server.on("/index.html", handleRootPath);
  server.begin();
  Serial.println("Server listening...");
}

void setup_filesystem() {
  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS.");
  } else {
    Serial.println("SPIFFS mounted.");
  }
}

void debug_payload(String payload) {
  if (debug) {
    Serial.println("received payload:\n<<");
    Serial.println(payload);
    Serial.println(">>");
  }
}

void handleRootPath() {
  String index = "";
  File file = SPIFFS.open("/index.html", "r");
  while (file.available()) {
    index.concat(file.readStringUntil('\n'));
  }
  file.close();

  if (current_state == STARTING || current_state == DEVICE_TOKEN_REQUEST) {
    DynamicJsonDocument doc(768);
    device_authorization_request(doc);
    if (!doc.isNull()) {
      String message = "To sign in, use a web browser to open the page <a target=\"_blank\" href=\"https://microsoft.com/devicelogin\">https://microsoft.com/devicelogin</a> and enter the code <em>%code%</em> to authenticate.";
      message.replace("%code%", doc["user_code"].as<String>());
      index.replace("%message%", message);
      server.send(200, "text/html", index);
      current_state = DEVICE_TOKEN_REQUEST;
    } else {
      index.replace("%message%", "Konnte 'devicecode' flow nicht starten.");
      server.send(200, "text/html", index);
    }
  }

  if (current_state == AUTHENICATED) {
    index.replace("%message%", "ESP8266 ist authentifiziert.");
    server.send(200, "text/html", index);

  }
}

void reset_indicator() {
  pixels.clear();
}

void presence_loop(JsonDocument &doc) {

  if (https.begin(*client, "https://graph.microsoft.com/beta/me/presence")) {
    String bearer = "Bearer ";
    bearer.concat(doc["access_token"].as<String>());
    https.addHeader("Authorization", bearer);
    int httpCode = https.GET();
    if (httpCode > 0) {
      DynamicJsonDocument presence(1024);
      // HTTP header has been send and Server response header has been handled
      if (httpCode == HTTP_CODE_OK) {
        const String& payload = https.getString();
        debug_payload(payload);
        deserializeJson(presence, payload);
        String activity = presence["activity"].as<String>();
        Serial.printf("Activity: %s\n", activity);
        if (activity.equalsIgnoreCase("InACall")) {
          reset_indicator();
          onAir();
        } else if (activity.equalsIgnoreCase("Available")) {
          reset_indicator();
          available();
        } else if (activity.equalsIgnoreCase("Offline")) {
          reset_indicator();
          offline();
        }
      }
    } else {
      Serial.printf("[HTTP] GET /beta/me/presence failed, error: %s\n", https.errorToString(httpCode).c_str());
    }
  }
}

void device_authorization_request(JsonDocument &doc) {

  if (https.begin(*client, "https://login.microsoftonline.com/" TENANT "/oauth2/v2.0/devicecode")) {
    https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpCode = https.POST("client_id=" CLIENT_ID "&scope=presence.read%20user.read%20offline_access%20openid%20profile%20email");
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] POST (device_authorization_request)... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        const String& payload = https.getString();
        debug_payload(payload);
        deserializeJson(doc, payload);
        strcpy(device_code, doc["device_code"].as<const char*>());
      }
    } else {
      Serial.printf("[HTTP] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }
  }
}

void token_request(JsonDocument &doc) {
  if (https.begin(*client, "https://login.microsoftonline.com/" TENANT "/oauth2/v2.0/token")) {
    https.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String body = "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Adevice_code&code=";
    body.concat(device_code);
    body.concat("&client_id=");
    body.concat(CLIENT_ID);

    int httpCode = https.POST(body);
    if (httpCode > 0) {
      Serial.printf("[HTTP] POST (token_request)... code: %d\n", httpCode);
      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        const String& payload = https.getString();
        debug_payload(payload);
        deserializeJson(doc, payload);
        File token = SPIFFS.open("token.json", "w");
        serializeJson(doc, token);
        token.close();
        current_state = AUTHENICATED;
      } else {
        Serial.printf("[HTTP] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
      }
    }
  }
}

void token_refresh_request(JsonDocument &doc) {
  if (https.begin(*client, "https://login.microsoftonline.com/" TENANT "/oauth2/v2.0/token")) {
    https.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String body = "grant_type=refresh_token&refresh_token=";
    body.concat(doc["refresh_token"].as<String>());
    body.concat("&client_id=");
    body.concat(CLIENT_ID);

    int httpCode = https.POST(body);
    if (httpCode > 0) {
      Serial.printf("[HTTP] POST (token_refresh_request)... code: %d\n", httpCode);
      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        const String& payload = https.getString();
        debug_payload(payload);
        File token = SPIFFS.open("token.json", "w");
        serializeJson(doc, token);
        token.close();
        current_state = AUTHENICATED;
      } else {
        Serial.printf("[HTTP] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
      }
    }
  }
}

void startup_check(JsonDocument &doc)
{
  File token = SPIFFS.open("token.json", "r");
  deserializeJson(doc, token);
  if (!doc.isNull()) {
    available();
    current_state = REFRESH_TOKEN;
  } else {
    error();
  }
}

void onAir() {
  pixels.fill(pixels.Color(220, 0, 0) , 0 , NUMPIXELS);
  pixels.show();
}

void available() {
  pixels.fill(pixels.Color(0, 220, 0) , 0 , NUMPIXELS);
  pixels.show();
  delay (100);
}

void offline() {
  pixels.fill(pixels.Color(220, 220, 0) , 0 , NUMPIXELS);
  pixels.show();
  delay (100);
}

void error() {
  for (int i = 0; i < NUMPIXELS; i = i + 2) {
    pixels.setPixelColor(i, pixels.Color(220, 0, 0));
  }
  pixels.show();
  delay (100);
  pixels.clear();

  for (int i = 1; i < NUMPIXELS; i = i + 2) {
    pixels.setPixelColor(i, pixels.Color(220, 0, 0));
  }
  pixels.show();
  delay (100);

  pixels.clear();
}


void loop()
{
  server.handleClient();
  MDNS.update();

  switch (current_state) {
    case STARTING:
      startup_check(token);
      break;
    case REFRESH_TOKEN:
      token_refresh_request(token);
      break;
    case AUTHENICATED:
      presence_loop(token);
      break;
    case DEVICE_TOKEN_REQUEST:
      token_request(token);
      break;
  }
  delay(500);
}
