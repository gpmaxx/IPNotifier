/*   A IP Notifier that emails (using IFTTT) when external IP address changes

    gpmaximus - 2019-05-29

    Note: Written to use the ESP-01 module. But the ESP-01 doesn't support
    deepsleep without some hardware modifiction. So this code is not suitable to run
    on battery.

    Switching to use the d1 mini or other esp8266 module, deepsleep functionality
    could be implemented.  Wake, check, sleep etc. with very little power use.
    Possibly able to run on battry for months or years.

*/

#define DEBUG 0

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
const char* IFTTT_KEY = "xxxxx";
const String IFTTT_EVENTNAME = "ipchange";
const char* AP_NAME = "IP_NOTE_CFG";

const uint32_t CHECK_INTERVAL_MS = 60 * 60 * 1000;   // once per hour
const uint32_t NOTIFY_INTERVAL_MS = 24 * 60 * 60 * 1000; // once per day

/*  Always - send IP notification every interval
    Interval - send IP notification once per interval or upon change whichever is first
    Change Only - send IP notificationn only when there is a new IP

    Note - IP notification will always be sent on initial power on
*/
enum NotificationMode {ALWAYS, INTERVAL, CHANGE_ONLY};
const NotificationMode NOTIFY_MODE = INTERVAL;

void error() {
  debugPrintln(F("Critical Error"));
  ESP.restart();
}

String getPreviousIP() {

  String theIP = "";

  fs::File file = SPIFFS.open(SPIFFS_DATAFILE,"r");

  if (file) {
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

  #ifndef DEBUG
    pinMode(LED_BUILTIN,OUTPUT);
    digitalWrite(LED_BUILTIN,LOW);
    delay(durationMS);
    digitalWrite(LED_BUILTIN,HIGH);
  #endif
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

  //indicateStart();
  String currentIP =  getCurrentIP();
  String previousIP = getPreviousIP();

  debugPrint(F("Current IP: "));
  debugPrintln(currentIP);
  debugPrint(F("Previous IP: "));
  debugPrintln(previousIP);

  bool shouldNotify = false;
  if (currentIP.compareTo(previousIP) != 0)  {
      debugPrintln(F("IP has changed"));
      shouldNotify = true;
  }
  else if (mandatoryNotify) {
      debugPrintln(F("First run mandatory notification"));
      shouldNotify = true;
  }
  else if (NOTIFY_MODE == ALWAYS) {
      debugPrintln(F("Always Notify mode"));
      shouldNotify = true;
  }
  else if (NOTIFY_MODE == INTERVAL) {
    debugPrintln(F("Interval Mode"));
    debugPrint(F("Interval (ms): "));
    debugPrint(NOTIFY_INTERVAL_MS);
    debugPrint(F(" Time elapsed (ms): "));
    uint32_t elapsed = millis() - dayTimer;
    debugPrintln(elapsed);
    shouldNotify = (elapsed >= NOTIFY_INTERVAL_MS);
  }
  if (shouldNotify) {
    saveIP(currentIP);
    notifyIP(previousIP,currentIP);
    dayTimer = millis();
  }
  else {
    debugPrintln(F("No notification criteria met"));
  }

//  indicateStop();

}

void setup() {

  //indicateStart();

  debugBegin(74880);
  debugPrintln(F("IP Notifier"));
  debugPrint(F("Notification Mode: "));

  switch (NOTIFY_MODE) {
    case ALWAYS:
      debugPrintln(F("Always notify"));
      break;
    case INTERVAL:
      debugPrint(F("Interval - Every "));
      debugPrint(NOTIFY_INTERVAL_MS);
      debugPrintln(F("ms"));
      break;
    case CHANGE_ONLY:
      debugPrintln(F("Only on change"));
      break;
  }

  WiFiManager wifiManager;
  wifiManager.autoConnect(AP_NAME);

  if (!SPIFFS.begin()) {
    error();
  }

  check(true);    // always send notification on first init

}

void loop() {

  debugPrint(F("Delay for "));
  debugPrint(CHECK_INTERVAL_MS);
  debugPrintln(F(" ms"));
  delay(CHECK_INTERVAL_MS);
  check(false);

}
