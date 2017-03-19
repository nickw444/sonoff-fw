#include <Ticker.h>
#include <Button.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <EEPROM.h>

#define TOPIC_REBOOT  "reboot"
#define TOPIC_RESET   "reset"

#define HOMEKIT_CALLBACK_SIGNATURE std::function<void(byte *, unsigned int)>

struct Subscription {
  HOMEKIT_CALLBACK_SIGNATURE cb;
  String topic;
};

struct WMSettings {
  uint16_t eepromSalt = 0x00;
  char mqttAddress[30] = "";
  char mqttUser[17] = "";
  char mqttPassword[17] = "";
  int mqttPort = 8883;
};

class Homekit {
  public:
    Homekit(uint8_t buttonPin, uint8_t ledPin, uint16_t eeprom_salt);
    void beginConfig();
    void tick();

    void subscribeTo(String topic, HOMEKIT_CALLBACK_SIGNATURE callback);
    void publish(String topic, byte * data);
    void onSaveConfig();

    void reboot();
    void reset();

    String getPlainMac(void);
    String hostname;

  private:
    Ticker ticker;
    Button* button;

    WiFiClientSecure espClient;
    PubSubClient* client;

    struct Subscription subscriptions[];
    struct WMSettings settings;

    uint16_t eepromSalt;


    void tickLED();
    void onEnterConfigMode(WiFiManager *wifi);



    void mqttCallback(char * topic, byte * payload, unsigned int length);
    void mqttReconnect();

    bool shouldSaveConfig = false;


    uint8_t buttonPin;
    uint8_t ledPin;
};
