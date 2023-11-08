/*
 * Sources: 
 * For code reference:
 * https://github.com/miguelbalboa/rfid
 *
 * For detecting RFID removal:
 * https://highvoltages.co/tutorial/arduino-tutorial/arduino-mfrc522-tutorial-is-rfid-tag-present-or-removed/
 *
 * For reading NFC data:
 * https://github.com/makeratplay/esp32SpotifyEchoDot/blob/main/esp32SpotifyEchoDot.ino
 * 
 * For HTTPS Post Request:
 * Examples of ESP8266HTTPClient (BasicHttpsClient and POSTHttpClient)
 *
 * For playing Spotify API:
 * https://github.com/witnessmenow/spotify-api-arduino
 */

#include "SpotifyCredentials.h"
#include <SPI.h>
#include <MFRC522.h>
#include <FS.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WiFiClientSecure.h>
#include <SpotifyArduino.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <time.h>

#define RST_PIN         D3
#define SS_PIN          D8
#define RED_PIN         D2
#define GREEN_PIN       D1
#define BLUE_PIN        D4

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

#define MDNS_DOMAIN "spotbox"
#define APSSID "Spot Box"
#define SPOTIFY_DETAILS_FILE "/spotifyauth.txt"
#define SPOTIFY_DEVICE_FILE "/spotifydevice.txt"

#define SPOTIFY_MARKET "PH"
#define SPOTIFY_SCOPE "user-read-playback-state%20user-modify-playback-state"
#define CALLBACK_URI "http%3A%2F%2Fspotbox.local%2Fcallback"

#define MY_NTP_SERVER "asia.pool.ntp.org"           
#define MY_TZ "PST-8"   

#define LED_OFF_HOUR 21
#define LED_ON_HOUR 6

unsigned long MAX_SPOTIFY_POLLING_TIME = 60 * 5 * 1000;
unsigned long MAX_SPOTIFY_POLLING_DELAY = 30 * 1000;
unsigned long MAX_NTP_POLLING_DELAY = 10 * 1000;

byte const BUFFERSiZE = 176;
uint8_t control = 0x00;
String deviceId = "";
bool isPlaying = false;
String spotifyDevicesHtml = "";

// Cache last played Spotify Id to trigger resume or restart correctly
String lastPlayingId = "";
unsigned long lastPlayingTime = 0;
bool shouldGetCurrentlyPlaying = false;
unsigned long lastPollTime = 0;

ESP8266WebServer server(80);
WiFiClientSecure client;
WiFiManager wm;
SpotifyArduino spotify(client, CLIENT_ID, CLIENT_SECRET);

time_t now;
tm tm;
unsigned long lastNTPPollTime = 0;
bool turnOffLED = false;

void(* resetFunc) (void) = 0;

String getAPSSID() {
  String macAddress = WiFi.macAddress();
  int macAddressLength = macAddress.length();
  String ending = macAddress.substring(macAddressLength - 5);
  ending.remove(2, 1);

  return String(APSSID) + " " + ending;
}

String getMDNSDomain() {
  return MDNS_DOMAIN;
}

String getIPAddress() {
  if (WiFi.isConnected()) {
    return WiFi.localIP().toString();
  } else {
    return WiFi.softAPIP().toString();
  }
}

String getSpotifyLoginWebpage() {
  char hrefValue[300];
  sprintf(hrefValue, "https://accounts.spotify.com/authorize?client_id=%s&response_type=code&redirect_uri=%s&scope=%s", CLIENT_ID, CALLBACK_URI, SPOTIFY_SCOPE);

  String htmlPrefix = "<html><head><meta charset=\"utf-8\"><meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no\"><title>Spot Box Spotify Login</title><style type=\"text/css\">body,html{min-height:100%}body{font-family:\"Helvetica Neue\",Helvetica,Arial,sans-serif;background-color:#000}h1{font-size:2em;margin-bottom:12px;margin-top:0}#login-container{display:-ms-flexbox;display:flex;height:100%;-ms-flex-direction:column;flex-direction:column;-ms-flex-align:center;align-items:center;margin:0 auto;-ms-flex-pack:center;justify-content:center}#login-container .login{color:#fff;text-align:center;padding:45px}#login-container .login .big-btn{margin:10px 0 20px 0}#login-container .login p{margin:5px 0;font-size:14px}.big-btn{color:#000;background-color:#1ed760;font-size:14px;line-height:1;border-radius:500px;padding:18px 48px 16px;transition-property:background-color;transition-duration:.3s;border-width:0;letter-spacing:2px;min-width:160px;text-transform:uppercase;white-space:normal;cursor:pointer;font-weight:700;text-decoration:none;display:inline-block}.big-btn:hover{background-color:#1fdf64}</style></head><body><div id=\"login-container\"><div class=\"login\"><h1>Spot Box</h1><a id=\"login-button\" href=\"";
  String htmlSuffix = "\" class=\"big-btn\">Log in with Spotify</a><p class=\"login-desc\">Please login to get access to spotify content.</p><p class=\"login-desc-small\">You will automatically be redirected to this page after login.</p></div></div></body></html>";

  return htmlPrefix + String(hrefValue) + htmlSuffix;
}

String getDeviceListWebpage(String listHtml) {
  String htmlPrefix = "<html><head><meta charset=\"utf-8\"><meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no\"><title>Select Device Player</title><style type=\"text/css\">body,html{min-height:100%}body{font-family:\"Helvetica Neue\",Helvetica,Arial,sans-serif;background-color:#000}h1{font-size:2.2em;margin-bottom:2px;margin-top:0}#login-container{display:-ms-flexbox;display:flex;min-height:100%;-ms-flex-direction:column;flex-direction:column;-ms-flex-align:center;align-items:center;margin:0 auto;-ms-flex-pack:center;justify-content:center}#login-container .login{color:#fff;text-align:center;padding:45px}.login-desc-small{margin:2px 0;font-size:14px}ul{margin:0;padding:0;list-style:none;margin-top:10px}li{padding:20px;border-top:1px solid #ccc}a{color:#fff;text-decoration:none}li:first-child{border:0}.selected{color:#1ed760;font-weight:700}</style></head><body><div id=\"login-container\"><div class=\"login\"><h1>Choose Device</h1><p class=\"login-desc-small\">Select the device you want to play the music on.</p><ul>";
  String htmlSuffix = "</ul></div></div></body></html>";

  return htmlPrefix + listHtml + htmlSuffix;
}

void setup() {
  // Serial.begin(115200);
  // while (!Serial);

  pinMode(RED_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);

  configTime(MY_TZ, MY_NTP_SERVER);

	SPI.begin();			// Init SPI bus
	mfrc522.PCD_Init();		// Init MFRC522
  client.setInsecure();

  Serial.println("Starting the Spot Box...");

  Serial.println("Mounting LittleFS...");
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    return;
  }

  if (!MDNS.begin(getMDNSDomain())) {
    Serial.println("Error setting up MDNS responder!");
    return;
  }
  Serial.println("mDNS responder started: " + getMDNSDomain() + ".local");

  setupCachedData();
  
  // Start TCP (HTTP) server
  server.on("/", handleRoot);
  server.on("/info", handleInfo);
  server.on("/spotifyDevices", handleSpotifyDevices);
  server.on("/callback", handleSpotifyCallback);
  server.on("/reset", HTTP_POST, reset);
  server.begin();
  Serial.println("HTTP server started");

  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);
}

void loop() {
  MDNS.update();
  server.handleClient();
  handleLEDDimming();
  
  // shouldGetCurrentlyPlaying:  Poll spotify for MAX_SPOTIFY_POLLING_TIME
  // isAllowedToPollAgain: The delay between API calls
  bool isAllowedToPollAgain = (millis() - lastPollTime > MAX_SPOTIFY_POLLING_DELAY);
  if (shouldGetCurrentlyPlaying && isAllowedToPollAgain) {
    lastPollTime = millis();
    getCurrentlyPlaying();
  }

  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  readNFCTag();

  while (true) {
    MDNS.update();
    server.handleClient();
    handleLEDDimming();

    control = 0;
    for (int i = 0; i < 3; i++) {
      if (!mfrc522.PICC_IsNewCardPresent()) {
        if (mfrc522.PICC_ReadCardSerial()) {
          control |= 0x16;
        }

        if (mfrc522.PICC_ReadCardSerial()) {
          control |= 0x16;
        }
        
        control += 0x1;
      }

      control += 0x4;
    }

    if (control == 13 || control == 14) {
      // Card still there
    } else {
      break;
    }
  }

  if (lastPlayingId != "") {
    pause();
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

void readNFCTag() {
  byte dataBuffer[BUFFERSiZE];
  readNFCTagData(dataBuffer);
  
  Serial.print("Read NFC tag: ");
  String nfcData = parseNFCTagData(dataBuffer);
  Serial.println(nfcData);

  if (nfcData.startsWith("open.spotify.com")) {
    play(nfcData);
  } else {
    Serial.println("Unrecognized NFC Data: " + nfcData);
  }
}

void readNFCTagData(byte *dataBuffer) {
  MFRC522::StatusCode status;
  byte byteCount;
  byte buffer[18];
  byte x = 0;

  int totalBytesRead = 0;

  // reset the dataBuffer
  for (byte i = 0; i < BUFFERSiZE; i++) {
    dataBuffer[i] = 0;
  }

  for (byte page = 0; page < BUFFERSiZE / 4; page += 4) {
    // Read pages
    byteCount = sizeof(buffer);
    status = mfrc522.MIFARE_Read(page, buffer, &byteCount);
    if (status == mfrc522.STATUS_OK) {
      totalBytesRead += byteCount - 2;

      for (byte i = 0; i < byteCount - 2; i++) {
        dataBuffer[x++] = buffer[i]; // add data output buffer
      }
    }
    else {
      break;
    }
  }
}

/*
  The first 28 bytes from the tag is a header info for the tag
  NFC data starts at position 29
*/
String parseNFCTagData(byte *dataBuffer) {
  // first 28 bytes is header info
  // data ends with 0xFE
  String retVal = "";
  for (int i = 28; i < BUFFERSiZE; i++) {
    if (dataBuffer[i] == 0xFE || dataBuffer[i] == 0x00) {
      break;
    }

    retVal += (char) dataBuffer[i];
  }
  return retVal;
}

void setupCachedData() {
  WiFi.mode(WIFI_STA);

  String hostname = getAPSSID();
  hostname.replace(" ", "");
  WiFi.hostname(hostname.c_str());

  updateConnectWiFiIndicator();
  bool res = wm.autoConnect(getAPSSID().c_str(), "nfcspotbox");
  if(!res) {
    resetFunc();
  }

  updateConnectSpotifyIndicator();

  String savedRefreshToken = getFileContents(SPOTIFY_DETAILS_FILE);
  if (savedRefreshToken != "") {
    syncSpotifyRefreshToken(savedRefreshToken.c_str());

    if (!spotify.refreshAccessToken()) {
      Serial.println("Failed to get access tokens");
      updateConnectSpotifyIndicator();
    } else {
      updateReadyIndicator(HIGH);
    }

    String savedDeviceId = getFileContents(SPOTIFY_DEVICE_FILE);
    if (savedDeviceId != "") {
      syncSpotifyDeviceId(savedDeviceId.c_str());
    }
  } else {
    updateConnectSpotifyIndicator();
  }
}

void syncSpotifyRefreshToken(const char *refreshToken) {
  spotify.setRefreshToken(refreshToken);

  Serial.print("Saving refreshToken: ");
  Serial.println(refreshToken);
  writeFile(SPOTIFY_DETAILS_FILE, refreshToken);
}

void syncSpotifyDeviceId(const char *_deviceId) {
  deviceId = String(_deviceId);
  
  Serial.print("Saving Device ID: ");
  Serial.println(_deviceId);
  writeFile(SPOTIFY_DEVICE_FILE, _deviceId);
}

void play(String spotifyUrl) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to WiFi");
    return;
  }

  spotifyUrl.remove(0, 17);
  spotifyUrl.replace('/', ':');
  
  String spotifyId = ("spotify:" + spotifyUrl);
  Serial.print("Playing ");
  Serial.println(spotifyId);

  isPlaying = true;
  shouldGetCurrentlyPlaying = false;
  lastPlayingTime = 0;

  updateLoadingIndicator();
  if (spotifyId != lastPlayingId) {
    spotifyPlayAlbumFromStart(spotifyId);
  } else {
    spotify.play(deviceId.c_str());
  }
  updateReadyIndicator(HIGH);
}

void pause() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to WiFi");
    return;
  }

  isPlaying = false;
  lastPlayingTime = millis();
  lastPollTime = millis();
  shouldGetCurrentlyPlaying = true;

  updateLoadingIndicator();
  spotify.pause();
  updateReadyIndicator(HIGH);
}

void getCurrentlyPlaying() {
  shouldGetCurrentlyPlaying = (millis() - lastPlayingTime < MAX_SPOTIFY_POLLING_TIME);

  int status = spotify.getCurrentlyPlaying(currentlyPlayingCallback, SPOTIFY_MARKET);
  if (status != 200 || !shouldGetCurrentlyPlaying) {
    lastPlayingId = "";
    lastPlayingTime = 0;
  }
}

void currentlyPlayingCallback(CurrentlyPlaying currentlyPlaying) {
  if (strcmp(currentlyPlaying.contextUri, lastPlayingId.c_str()) != 0) {
    lastPlayingId = "";
    shouldGetCurrentlyPlaying = false;
  }
}

void spotifyPlayAlbumFromStart(String albumId) {
  char body[100];
  sprintf(body, "{\"context_uri\" : \"%s\"}", albumId.c_str());
  spotify.playAdvanced(body, deviceId.c_str());
  lastPlayingId = albumId;
}

String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++) {
    if(data.charAt(i)==separator || i==maxIndex) {
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void handleInfo() {
  server.send(200, "application/json", "{ \"mdns\": \"" + getMDNSDomain() + ".local\", \"ipaddress\": \"" + getIPAddress() + "\", \"success\": true }");
}

void handleRoot() {
  String savedRefreshToken = getFileContents(SPOTIFY_DETAILS_FILE);
  if (savedRefreshToken == "") {
    server.send(200, "text/html", getSpotifyLoginWebpage());
  } else {
    server.sendHeader("Location", "/spotifyDevices",true);
    server.send(302, "text/plain", "");
  }
}

void handleSpotifyDevices() {
  String deviceId = server.arg("deviceId");
  if (deviceId != "") {
    syncSpotifyDeviceId(deviceId.c_str());
  }

  int status = spotify.getDevices(getDeviceCallback);
  if (status != 200) {
    server.send(404, "text/plain", "Failed to get Spotify Devices");
  }
}

bool getDeviceCallback(SpotifyDevice device, int index, int numDevices) {
  if (index == 0) {
    spotifyDevicesHtml = "";
  }

  if (String(device.id) == deviceId) {
    spotifyDevicesHtml += "<li><a href=\"/spotifyDevices?deviceId=" + String(device.id) + "\" class=\"selected\">" + String(device.name) + "</a></li>";
  } else {
    spotifyDevicesHtml += "<li><a href=\"/spotifyDevices?deviceId=" + String(device.id) + "\">" + String(device.name) + "</a></li>";
  }

  if (index == numDevices - 1) {
    server.send(200, "text/html", getDeviceListWebpage(spotifyDevicesHtml));
    if (deviceId != "") {
      Serial.println(isPlaying);
      spotify.transferPlayback(deviceId.c_str(), isPlaying); 
    }

    return false;
  }

  return true;
}

void handleSpotifyCallback() {
  const char *refreshToken = NULL;
  String code = server.arg("code");
  if (code != "") {
    refreshToken = spotify.requestAccessTokens(code.c_str(), CALLBACK_URI);
    syncSpotifyRefreshToken(refreshToken);
  }

  if (refreshToken != NULL) {
    updateReadyIndicator(HIGH);
    handleSpotifyDevices();
  } else {
    server.send(404, "text/plain", "Failed to load token, check serial monitor");
  }
}

void reset() {  
  server.send(200, "application/json", "{ \"message\": \"Resetting and restarting device.\", \"success\": true }");

  delay(3000);
  LittleFS.format();
  wm.resetSettings();
  resetFunc();
}

String getFileContents(const char *path) {
  Serial.printf("Reading file: %s\n", path);

  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return "";
  }

  String contents = file.readString();
  Serial.print("Read from file: ");
  Serial.println(contents);
  
  file.close();

  return contents;
}

void writeFile(const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);

  File file = LittleFS.open(path, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void updateConnectWiFiIndicator() {
  digitalWrite(RED_PIN, HIGH);
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(BLUE_PIN, LOW);
}

void updateConnectSpotifyIndicator() {
  digitalWrite(RED_PIN, LOW);
  digitalWrite(GREEN_PIN, HIGH);
  digitalWrite(BLUE_PIN, LOW);
}

void updateLoadingIndicator() {
  digitalWrite(RED_PIN, HIGH);
  digitalWrite(GREEN_PIN, HIGH);
  digitalWrite(BLUE_PIN, LOW);
}

void updateReadyIndicator(uint8_t value) {
  digitalWrite(RED_PIN, value);
  digitalWrite(GREEN_PIN, value);
  digitalWrite(BLUE_PIN, value);
}

void handleLEDDimming() {
  if (deviceId == "") return;
  bool isAllowedToPollAgain = (millis() - lastNTPPollTime > MAX_NTP_POLLING_DELAY);
  if (isAllowedToPollAgain) {
    lastNTPPollTime = millis();

    time(&now);
    localtime_r(&now, &tm);
    
    if (tm.tm_hour >= LED_OFF_HOUR) {
      updateReadyIndicator(LOW);
    } else if (tm.tm_hour >= LED_ON_HOUR) {
      updateReadyIndicator(HIGH);
    }
  }
}
