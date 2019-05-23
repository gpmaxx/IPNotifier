/*   A IP Notifier that emails when external IP address changes */

/*  Notes - the LED functionality only works properly if Serial is disabled
    as the onboard LED using the TX pin

    ToDo: Test the 3 modes
          Test with uploading to new device
            - do you need to uploadFS first?
*/



#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#define FS_NO_GLOBALS
#include "FS.h"
#include "IFTTTMaker.h"

const char* SPIFFS_DATAFILE = "prevIP.dat";
const char* IP_HOST = "api.ipify.org";
const uint16_t IP_PORT = 80;
const uint32_t SLEEP_INTERVAL_MS = 60 * 60 * 1000;  // 1 hour ;
const char* IFTTT_KEY = "bZYS4W-feP_hGiN5ViibKkcNEF_Y24wdymZPq4HQ5d7";
const String IFTTT_EVENTNAME = "ipchange";
const uint16_t PER_DAY_INTERVAL_MS = 24 * 60 * 60 * 1000;

#define DEBUG 

#ifdef DEBUG
  #define debugBegin(...) Serial.begin(__VA_ARGS__)
  #define debugPrint(...) Serial.print(__VA_ARGS__)
  #define debugPrintln(...) Serial.println(__VA_ARGS__)
  #define debugPrintf(...) Serial.printf(__VA_ARGS__)
#else
  #define debugBegin(...)
  #define debugPrint(...)
  #define debugPrintln(...)
  #define debugPrintf(...)
#endif


/*  Always - send IP notification every interval
    Per Day - send IP notification once per 24 hours or on change
    Change Only - send IP notificationn only when there is a new IP

    Note - IP notification will always be sent on initial power on
*/
enum NotificationMode {ALWAYS, PER_DAY, CHANGE_ONLY};
const NotificationMode NOTIFY_MODE = PER_DAY;

void error() {
    pinMode(LED_BUILTIN,OUTPUT);
    while (true) {
      digitalWrite(LED_BUILTIN,LOW);
      delay(500);
      digitalWrite(LED_BUILTIN,HIGH);
      delay(500);
    }
}

void error(const char* errorMsg) {
  debugPrintln(errorMsg);
  error();
}

String getPreviousIP() {

  String theIP = "";

  fs::File file = SPIFFS.open(SPIFFS_DATAFILE,"r");

  if (!file) {
    debugPrintln(F("Error opening data file for read"));
  }
  else {
    while (file.available()) {
        theIP += file.readString();
    }
  }

  file.close();
  return theIP;

}

String getCurrentIP() {

  String theIP;
  WiFiClient client;
  String queryString;

  client.setTimeout(5000);
  if (client.connect(IP_HOST,IP_PORT)) {
    queryString = "GET / HTTP/1.0";
    client.println(queryString);
    queryString = "Host: ";
    queryString += IP_HOST;
    client.println(queryString);
    client.println(F("Connection: close"));
    if (client.println() == 0) {
      debugPrint(F("Failed to send request\r\n"));
    }

  }
  else {
    debugPrint(F("connection failed\r\n\r\n")); //error message if no client connect
    error();
  }

  while(client.connected() && !client.available()) delay(1);

  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    debugPrint(F("Invalid response\r\n"));
    error();
  }

  while (client.available()) {
    theIP += client.readString();
  }

  client.stop();
  return theIP;

}

bool saveIP(const String newIP) {

  String theIP;

  fs::File file = SPIFFS.open(SPIFFS_DATAFILE,"w");

  if (!file) {
    debugPrintln(F("Error opening data file for write"));
    error();
  }
  else {
    file.print(newIP);
  }

  file.close();
  return true;

}

void notifyIP(const String oldIP, const String newIP) {
  debugPrint(F("You IP address has changed from "));
  debugPrint(oldIP);
  debugPrint(F(" to "));
  debugPrintln(newIP);

  WiFiClient client;
  IFTTTMaker ifttt(IFTTT_KEY,client);

  if (ifttt.triggerEvent(IFTTT_EVENTNAME,newIP,oldIP)) {
    debugPrintln(F("successfully sent"));
  }
  else {
    debugPrintln(F("failed to notify"));
  }

}

void indicate(const uint16_t durationMS) {
  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,LOW);
  delay(durationMS);
  digitalWrite(LED_BUILTIN,HIGH);
}

void indicateStart() {
  indicate(5000);
}

void indicateStop() {
  indicate(3000);
}

void notify(const String previousIP, const String currentiP) {

}

void check(const bool mandatoryNotify) {

  static uint32_t dayTimer = 0;

  indicateStart();
  String currentIP =  getCurrentIP();
  String previousIP = getPreviousIP();

  debugPrint(F("Current IP: "));
  debugPrintln(currentIP);
  debugPrint(F("Previous IP: "));
  debugPrintln(previousIP);

  if (mandatoryNotify || (NOTIFY_MODE == ALWAYS) || (currentIP.compareTo(previousIP) != 0) || ((NOTIFY_MODE == PER_DAY) && ((millis() - dayTimer) > PER_DAY_INTERVAL_MS)))  {
    saveIP(currentIP);
    notifyIP(previousIP,currentIP);
    dayTimer = millis();
  }
  else {
    debugPrintln(F("no change to IP address"));
  }

  indicateStop();

}

void setup() {

  indicateStart();

  debugBegin(74880);
  debugPrintln(F("IP Notifier"));

  WiFiManager wifiManager;
  wifiManager.autoConnect("ESP32_connect");

  if (!SPIFFS.begin()) {
    error();
  }

  check(true);    // always send notification on first init

}

void loop() {

  debugPrint(F("Delay for "));
  debugPrint(SLEEP_INTERVAL_MS);
  debugPrintln(F(" seconds"));
  delay(SLEEP_INTERVAL_MS);
  check(false);

}
