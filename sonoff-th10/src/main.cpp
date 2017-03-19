/*
   1MB flash sizee
   sonoff header
   1 - vcc 3v3
   2 - rx
   3 - tx
   4 - gnd
   5 - gpio 14
   esp8266 connections
   gpio  0 - button
   gpio 12 - relay
   gpio 13 - green led - active low
   gpio 14 - pin 5 on header
*/

#define SONOFF_BUTTON    0
#define SONOFF_RELAY    12
#define SONOFF_LED      13

#define EEPROM_SALT 1263

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <EEPROM.h>
#include "DHT.h"
#include <Homekit-Sonoff.h>

#define DHTPIN 14
#define DHTTYPE DHT21


static Button button(0, false, true, 20);


static Homekit homekit(SONOFF_BUTTON, SONOFF_LED, EEPROM_SALT);
static DHT dht(DHTPIN, DHTTYPE);

void republish(byte * payload, unsigned int length);

void setup() {
  Serial.begin(115200);
  dht.begin();

  homekit.subscribeTo("republish", republish);
  homekit.beginConfig();
}

void republish(byte * payload, unsigned int length) {

}

void loop() {
  // Wait a few seconds between measurements.
  delay(2000);

  homekit.tick();

  homekit.publish("humidity", NULL);
  homekit.publish("temperature", NULL);
  homekit.publish("heatindex", NULL);


  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" *C ");
  Serial.print(f);
  Serial.print(" *F\t");
  Serial.print("Heat index: ");
  Serial.print(hic);
  Serial.print(" *C ");
  Serial.print(hif);
  Serial.println(" *F");
}
