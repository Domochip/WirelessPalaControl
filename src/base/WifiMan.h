#ifndef WifiMan_h
#define WifiMan_h

#include "..\Main.h"
#include "Application.h"

#include <Ticker.h>

#include "data\statusw.html.gz.h"
#include "data\configw.html.gz.h"

const char predefPassword[] PROGMEM = "ewcXoCt4HHjZUvY0";

class WifiMan : public Application
{

private:
  //Configuration Properties
  char ssid[32 + 1] = {0};
  char password[64 + 1] = {0};
  char hostname[24 + 1] = {0};
  uint32_t ip = 0;
  uint32_t gw = 0;
  uint32_t mask = 0;
  uint32_t dns1 = 0;
  uint32_t dns2 = 0;

  //Run properties
  WiFiEventHandler _discoEventHandler;
  WiFiEventHandler _staConnectedHandler;
  WiFiEventHandler _staDisconnectedHandler;
  int _apChannel = 2;
  char _apSsid[64];
  bool _needRefreshWifi = false;
  bool _stationConnectedToSoftAP = false;
  Ticker _refreshTicker;
  uint8_t _refreshPeriod = 60; //try to reconnect as station mode every 60 seconds
  uint8_t _reconnectDuration = 20; //duration to try to connect to Wifi in seconds

  void enableAP(bool force);
  void refreshWiFi();

  void setConfigDefaultValues();
  void parseConfigJSON(DynamicJsonDocument &doc);
  bool parseConfigWebRequest(AsyncWebServerRequest *request);
  String generateConfigJSON(bool forSaveFile);
  String generateStatusJSON();
  bool appInit(bool reInit);
  const uint8_t *getHTMLContent(WebPageForPlaceHolder wp);
  size_t getHTMLContentSize(WebPageForPlaceHolder wp);
  void appInitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication);
  void appRun();

public:
  WifiMan(char appId, String appName) : Application(appId, appName) {}
};

#endif
