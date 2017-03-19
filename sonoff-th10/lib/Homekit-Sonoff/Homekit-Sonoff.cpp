#include "Homekit-Sonoff.h"

Homekit::Homekit(uint8_t buttonPin, uint8_t ledPin, uint16_t eepromSalt) {
  this->client = new PubSubClient(espClient);
  this->button = new Button(buttonPin, false, true, 20);

  hostname = "Sonoff-" + getPlainMac();

  this->ledPin = ledPin;
  this->eepromSalt = eepromSalt;

}

void Homekit::beginConfig() {
  pinMode(ledPin, OUTPUT);


  WiFiManager wifiManager;
  // wifiManager.setAPCallback(std::bind(&Homekit::onEnterConfigMode, this));
  wifiManager.setConfigPortalTimeout(180); //Reboot if it's not configured.

  // Handle Config Params
  EEPROM.begin(512);
  EEPROM.get(0, settings);
  EEPROM.end();

  if (settings.eepromSalt != eepromSalt) {
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
  // wifiManager.setSaveConfigCallback(std::bind(&Homekit::onSaveConfig, this));

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


  // subscribeTo(TOPIC_REBOOT, std::bind(&Homekit::reboot, this));
  // subscribeTo(TOPIC_RESET, std::bind(&Homekit::reset, this);

  client->setServer(settings.mqttAddress, settings.mqttPort);
  // client->setCallback(std::bind(&Homekit::mqttCallback, this));
}

void Homekit::tick() {
  if (!client->connected()) {
    mqttReconnect();
  }

  client->loop();
  button->read();

  if (button->pressedFor(10000)) {
    Serial.println("Reset Settings");
    reset();
  } else if (button->wasReleased()) {
    // TODO: ON Button Press Hook.
  }
}

void Homekit::subscribeTo(String topic, HOMEKIT_CALLBACK_SIGNATURE callback) {
  // TODO
}

void Homekit::reboot() {
  ESP.reset();
  delay(2000);
}

void Homekit::reset() {
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

String getPlainMac(void) {
  byte mac[6];
  WiFi.macAddress(mac);
  String sMac = "";
  for (int i = 0; i < 6; ++i) {
    sMac += String(mac[i], HEX);
  }
  return sMac;
}


void Homekit::publish(String topic, byte * data) {
  // client->publish(const char *topic, const char *payload)
}

void Homekit::onEnterConfigMode(WiFiManager *wifi) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(wifi->getConfigPortalSSID());
  //entered config mode, make led toggle faster

  std::function<void(void)> callback = std::bind(&Homekit::tickLED, this);
  ticker.attach(0.2, callback);
  // ticker.attach(0.2, [=]() {
  //   Serial.println("Hello\n");
  //   tickLED();
  // });
}

void Homekit::onSaveConfig() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void Homekit::mqttReconnect() {
  Serial.println("Attempting MQTT connection...");
  // Create a random client ID
  String clientId = "esp-";
  clientId += getPlainMac();

  // Attempt to connect. We will setup a will topic publish so that when
  // the device disconnects, it will set it's state to off.
  if (client->connect(clientId.c_str(), settings.mqttUser, settings.mqttPassword, topicRelayState.c_str(), 0, false, "0")) {
    Serial.println("Connected to MQTT");

    // TODO Subscribe to all topics.

    // client->subscribe(topicReboot.c_str());
    // client->subscribe(topicRelaySet.c_str());
    // client->subscribe(topicRepublish.c_str());
    // client->subscribe(topicReset.c_str());
    Serial.println("Subscribed to topics");
    // TODO: On re-connect callback to allow state notifications.
    Serial.println("Notified of current state");
  } else {
    Serial.print("failed, rc=");
    Serial.print(client->state());
    Serial.println(" try again in 5 seconds");
    // Wait 5 seconds before retrying
    delay(5000);
  }
}

void Homekit::mqttCallback(char *topic, byte *payload, unsigned int length) {
  Serial.printf("Message arrived [%s]\n", topic);
  //
  // if(strcmp(topic, topicReboot.c_str()) == 0) {
  //   Serial.println("Reboot was requested.");
  //   reboot();
  // } else if (strcmp(topic, topicRelaySet.c_str()) == 0) {
  //   if (payload[0] == '1') {
  //     Serial.println("Turning on.");
  //     turnOn();
  //   } else if (payload[0] == '0') {
  //     Serial.println("Turning off.");
  //     turnOff();
  //   } else {
  //     Serial.println("Invalid payload provided.");
  //   }
  // } else if (strcmp(topic, topicRepublish.c_str()) == 0) {
  //   Serial.println("Republish was requested.");
  //   notifyState();
  // } else if (strcmp(topic, topicReset.c_str()) == 0) {
  //   Serial.println("Reset was requested.");
  //   reset();
  // }
}
