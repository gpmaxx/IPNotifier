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
//const uint32_t SLEEP_INTERVAL = 1 * 60 * 60 * 1000 * 1000;  // 1 hour in uSeconds
const uint32_t SLEEP_INTERVAL = 30 * 1000000;
const char* IFTTT_KEY = "bZYS4W-feP_hGiN5ViibKkcNEF_Y24wdymZPq4HQ5d7";
const String IFTTT_EVENTNAME = "ipchange";
const bool ALWAYS_NOTIFY = true;

void deepSleep(const uint32_t interval) {
  #ifdef ESP01
    Serial.println("Deepsleep not supported");
    Serial.print("Delaying for ");
    Serial.print(interval / 1000000);
    Serial.print(" seconds");
    delay(interval/1000);
    ESP.restart();
  #else
    Serial.print("Going to sleep for ");
    Serial.print(interval / 1000000);
    Serial.println(" seconds");
    ESP.deepSleep(interval);
  #endif
}

void error() {
    delay(2000);
    deepSleep(SLEEP_INTERVAL);
}

void error(const char* errorMsg) {
  Serial.println(errorMsg);
  error();
}

String getPreviousIP() {

  String theIP = "";

  fs::File file = SPIFFS.open(SPIFFS_DATAFILE,"r");

  if (!file) {
    Serial.println("Error opening data file for read");
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
      Serial.print(F("Failed to send request\r\n"));
    }

  }
  else {
    Serial.print(F("connection failed\r\n\r\n")); //error message if no client connect
    error();
  }

  while(client.connected() && !client.available()) delay(1);

  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    Serial.print(F("Invalid response\r\n"));
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
    Serial.println("Error opening data file for write");
    error();
  }
  else {
    file.print(newIP);
  }

  file.close();
  return true;

}

void notifyIP(const String oldIP, const String newIP) {
  Serial.print("You IP address has changed from ");
  Serial.print(oldIP);
  Serial.print(" to ");
  Serial.println(newIP);

  WiFiClient client;
  IFTTTMaker ifttt(IFTTT_KEY,client);

  if (ifttt.triggerEvent(IFTTT_EVENTNAME,newIP,oldIP)) {
    Serial.println("successfully sent");
  }
  else {
    Serial.println("failed to notify");
  }

}

void setup() {

  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,LOW);
  delay(3000);
  digitalWrite(LED_BUILTIN,HIGH);

  Serial.begin(74880);
  Serial.println(F("IP Notifier"));

  WiFiManager wifiManager;
  wifiManager.autoConnect("ESP32_connect");

  if (!SPIFFS.begin()) {
    error();
  }

  String currentIP =  getCurrentIP();
  String previousIP = getPreviousIP();

  Serial.print("Current IP: ");
  Serial.println(currentIP);
  Serial.print("Previous IP: ");
  Serial.println(previousIP);

  if ((ALWAYS_NOTIFY) || (currentIP.compareTo(previousIP) != 0)) {
    saveIP(currentIP);
    notifyIP(previousIP,currentIP);
  }
  else {
    Serial.println("no change to IP address");
  }

  digitalWrite(LED_BUILTIN,LOW);
  delay(1000);
  digitalWrite(LED_BUILTIN,HIGH);
  deepSleep(SLEEP_INTERVAL);

}

void loop() {

  // we should never get here

  Serial.println("loop");


}
