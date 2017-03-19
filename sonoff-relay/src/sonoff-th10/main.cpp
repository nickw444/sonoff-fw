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
#define SONOFF_INPUT    14

#define EEPROM_SALT 1263

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <EEPROM.h>
#include <Button.h>

enum relayState {
  RELAY_STATE_ON = HIGH,
  RELAY_STATE_OFF = LOW,
};


typedef struct {
  int   salt = EEPROM_SALT;
  char  mqttAddress[30] = "";
  char  mqttUser[17] = "";
  char  mqttPassword[17] = "";
  int   mqttPort = 8883;
} WMSettings;

static WMSettings settings;
static enum relayState currentState;
static Ticker ticker;
static Button button(SONOFF_BUTTON, false, true, 20);
static bool shouldSaveConfig = false;

static WiFiClientSecure espClient;
static PubSubClient client(espClient);

static String topicRelayState;
static String topicRelaySet;
static String topicReboot;
static String topicRepublish;
static String topicReset;


void ledTick();
void setState(enum relayState s);
void setState(enum relayState s, bool notify);
void onEnterConfigMode (WiFiManager *wifi);
void onSaveConfig();
void toggle();
void reboot();
void reset();
String getPlainMac(void);
void makeTopicStrings();
void notifyState();

void mqttReconnect();
void mqttCallback(char* topic, byte* payload, unsigned int length);

void setup() {
  Serial.begin(115200);


  makeTopicStrings();

  //set led pin as output
  pinMode(SONOFF_LED, OUTPUT);
  pinMode(SONOFF_RELAY, OUTPUT);

  // Set the relay state so it's immediately on, but don't notify.
  setState(RELAY_STATE_ON, false);

  // Start a ticker to show that we're in config mode on the LED.
  ticker.attach(0.2, ledTick);

  WiFiManager wifiManager;
  wifiManager.setAPCallback(onEnterConfigMode);
  wifiManager.setConfigPortalTimeout(180); //Reboot if it's not configured.

  // Handle Config Params
  EEPROM.begin(512);
  EEPROM.get(0, settings);
  EEPROM.end();

  if (settings.salt != EEPROM_SALT) {
    Serial.println("Invalid settings in EEPROM, trying with defaults");
    WMSettings defaults;
    settings = defaults;
  }

  WiFiManagerParameter mqttServerAddress("mqtt-server-address", "MQTT Server Address", settings.mqttAddress, 30);
  WiFiManagerParameter mqttServerPort("mqtt-server-port", "MQTT Server Port", String(settings.mqttPort).c_str(), 6);
  WiFiManagerParameter mqttUsername("mqtt-username", "MQTT User", settings.mqttUser, 16);
  WiFiManagerParameter mqttPassword("mqtt-password", "MQTT Password", settings.mqttPassword, 16);
  wifiManager.addParameter(&mqttServerAddress);
  wifiManager.addParameter(&mqttServerPort);
  wifiManager.addParameter(&mqttUsername);
  wifiManager.addParameter(&mqttPassword);

  //set config save notify callback
  wifiManager.setSaveConfigCallback(onSaveConfig);

  String hostname = "Sonoff-" + getPlainMac();

  if (!wifiManager.autoConnect(hostname.c_str())) {
    Serial.println("failed to connect and hit timeout");
    reboot();
  }

  if (shouldSaveConfig) {
    Serial.println("Saving config");

    strcpy(settings.mqttAddress, mqttServerAddress.getValue());
    strcpy(settings.mqttUser, mqttUsername.getValue());
    strcpy(settings.mqttPassword, mqttPassword.getValue());
    settings.mqttPort = atoi(mqttServerPort.getValue());

    EEPROM.begin(512);
    EEPROM.put(0, settings);
    EEPROM.end();
  }

  ticker.detach(); // Stop Blinking LED
  Serial.println("Device is started...");
  Serial.printf("settings.mqttAddress: '%s'\n", settings.mqttAddress);
  Serial.printf("settings.mqttPort: '%d'\n", settings.mqttPort);
  Serial.printf("settings.mqttUser: '%s'\n", settings.mqttUser);
  Serial.printf("settings.mqttPassword: '%s'\n", settings.mqttPassword);
  Serial.printf("topicRelaySet: '%s'\n", topicRelaySet.c_str());
  Serial.printf("topicRelayState: '%s'\n", topicRelayState.c_str());
  Serial.printf("topicReboot: '%s'\n", topicReboot.c_str());

  setState(RELAY_STATE_ON);

  // Connect to MQTT
  client.setServer(settings.mqttAddress, settings.mqttPort);
  client.setCallback(mqttCallback);

}

void ledTick() {
  //toggle state
  int state = digitalRead(SONOFF_LED);  // get the current state of GPIO1 pin
  digitalWrite(SONOFF_LED, !state);     // set pin to the opposite state
}


void loop() {
  if (!client.connected()) {
    mqttReconnect();
  }

  client.loop();
  button.read();

  if (button.pressedFor(10000)) {
    Serial.println("Reset Settings");
    reset();
  } else if (button.wasReleased()) {
    Serial.println("Toggle Relay");
    toggle();
  }
}

void setState(enum relayState s, bool notify) {
  Serial.printf("Relay State Is %s\n", s == RELAY_STATE_ON ? "On" : "Off");
  currentState = s;
  digitalWrite(SONOFF_RELAY, s);
  digitalWrite(SONOFF_LED, (s + 1) % 2); // led is active low

  if (notify) {
    notifyState();
  }
}

void setState(enum relayState s) {
  setState(s, true);
}


void turnOn() {
  setState(RELAY_STATE_ON);
}

void turnOff() {
  setState(RELAY_STATE_OFF);
}

void toggle() {
  setState(currentState == RELAY_STATE_ON ? RELAY_STATE_OFF : RELAY_STATE_ON);
}

void reboot() {
  ESP.reset();
  delay(2000);
}

void reset() {

  WMSettings defaults;
  settings = defaults;
  EEPROM.begin(512);
  EEPROM.put(0, settings);
  EEPROM.end();

  WiFi.disconnect();
  delay(1000);
  ESP.reset();
  delay(2000);
}


void onEnterConfigMode (WiFiManager *wifi) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(wifi->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, ledTick);
}

void onSaveConfig() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void mqttReconnect() {
  Serial.println("Attempting MQTT connection...");
  // Create a random client ID
  String clientId = "esp-";
  clientId += getPlainMac();

  // Attempt to connect. We will setup a will topic publish so that when
  // the device disconnects, it will set it's state to off.
  if (client.connect(clientId.c_str(), settings.mqttUser, settings.mqttPassword, topicRelayState.c_str(), 0, false, "0")) {
    Serial.println("Connected to MQTT");

    client.subscribe(topicReboot.c_str());
    client.subscribe(topicRelaySet.c_str());
    client.subscribe(topicRepublish.c_str());
    client.subscribe(topicReset.c_str());
    Serial.println("Subscribed to topics");
    notifyState();
    Serial.println("Notified of current state");

  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");
    // Wait 5 seconds before retrying
    delay(5000);
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("Message arrived [%s]\n", topic);

  if(strcmp(topic, topicReboot.c_str()) == 0) {
    Serial.println("Reboot was requested.");
    reboot();
  } else if (strcmp(topic, topicRelaySet.c_str()) == 0) {
    if (payload[0] == '1') {
      Serial.println("Turning on.");
      turnOn();
    } else if (payload[0] == '0') {
      Serial.println("Turning off.");
      turnOff();
    } else {
      Serial.println("Invalid payload provided.");
    }
  } else if (strcmp(topic, topicRepublish.c_str()) == 0) {
    Serial.println("Republish was requested.");
    notifyState();
  } else if (strcmp(topic, topicReset.c_str()) == 0) {
    Serial.println("Reset was requested.");
    reset();
  }
}


String getPlainMac(void) {
  byte mac[6];
  WiFi.macAddress(mac);
  String sMac = "";
  for (int i = 0; i < 6; ++i) {
    sMac += String(mac[i], HEX);
  }
  return sMac;
}

void makeTopicStrings() {
  String macAddress = getPlainMac();
  topicRelayState = "device/" + macAddress + "/relay";
  topicRelaySet = "device/" + macAddress + "/relay/set";
  topicReboot = "device/" + macAddress + "/reboot";
  topicRepublish = "device/" + macAddress + "/republish";
  topicReset = "device/" + macAddress + "/reset";
}

void notifyState() {
  client.publish(topicRelayState.c_str(), currentState == RELAY_STATE_ON ? "1" : "0");
}
