#ifndef WirelessPalaControl_h
#define WirelessPalaControl_h

#include "Main.h"
#include "base/Version.h" //for BASE_VERSION define
#include "base/Utils.h"
#include "base/MQTTMan.h"
#include "base/Application.h"

const char appDataPredefPassword[] PROGMEM = "ewcXoCt4HHjZUvY1";

#include "data/status1.html.gz.h"
#include "data/config1.html.gz.h"

#include <Palazzetti.h>
#include <WiFiUdp.h>

class WebPalaControl : public Application
{
private:
#define HA_MQTT_GENERIC 0
#define HA_MQTT_GENERIC_JSON 1
#define HA_MQTT_GENERIC_CATEGORIZED 2

  typedef struct
  {
    byte type = HA_MQTT_GENERIC_JSON;
    uint32_t port = 1883;
    char username[128 + 1] = {0};
    char password[150 + 1] = {0};
    struct
    {
      char baseTopic[64 + 1] = {0};
    } generic;
    bool hassDiscoveryEnabled = true;
    char hassDiscoveryPrefix[64 + 1] = {0};
  } MQTT;

#define HA_PROTO_DISABLED 0
#define HA_PROTO_MQTT 1

  typedef struct
  {
    byte protocol = HA_PROTO_DISABLED;
    char hostname[64 + 1] = {0};
    uint16_t uploadPeriod = 60;
    MQTT mqtt;
  } HomeAutomation;

  HomeAutomation _ha;
  int _haSendResult = 0;
  WiFiClient _wifiClient;
  MQTTMan _mqttMan;
  WiFiUDP _udpServer;

  Palazzetti _Pala;
  unsigned long _lastAllStatusRefreshMillis = 0;

  bool _needPublish = false;
  Ticker _publishTicker;
  bool _publishedStoveConnected = false;
  bool _needPublishHassDiscovery = false; // set to true when MQTT connection is established

  int myOpenSerial(uint32_t baudrate);
  void myCloseSerial();
  int mySelectSerial(unsigned long timeout);
  size_t myReadSerial(void *buf, size_t count);
  size_t myWriteSerial(const void *buf, size_t count);
  int myDrainSerial();
  int myFlushSerial();
  void myUSleep(unsigned long usecond);

  void mqttConnectedCallback(MQTTMan *mqttMan, bool firstConnection);
  void mqttDisconnectedCallback();
  void mqttCallback(char *topic, uint8_t *payload, unsigned int length);
  void publishStoveConnectedToMqtt(bool stoveConnected);
  bool publishDataToMqtt(const String &baseTopic, const String &palaCategory, const JsonDocument &jsonDoc);
  bool publishHassDiscoveryToMqtt();
  bool executePalaCmd(const String &cmd, String &strJson, bool publish = false);

  void publishTick();
  void udpRequestHandler(WiFiUDP &udpServer);

  void setConfigDefaultValues();
  void parseConfigJSON(JsonDocument &doc, bool fromWebPage);
  bool parseConfigWebRequest(WebServer &server);
  String generateConfigJSON(bool forSaveFile);
  String generateStatusJSON();
  bool appInit(bool reInit);
  const PROGMEM char *getHTMLContent(WebPageForPlaceHolder wp);
  size_t getHTMLContentSize(WebPageForPlaceHolder wp);
  void appInitWebServer(WebServer &server, bool &shouldReboot, bool &pauseApplication);
  void appRun();

public:
  WebPalaControl(char appId, String fileName);
};

#endif
