#ifndef Application_h
#define Application_h

#include "../Main.h"
#include <LittleFS.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <Ticker.h>

// Maximum size that can be allocated to Parsed JSON
#define JSON_DOC_MAX_MEM_SIZE 4096
#define JSON_DOC_MEM_STEP 256

class Application
{
protected:
  typedef enum
  {
    status,
    config,
    fw,
    mn,
    discover
  } WebPageForPlaceHolder;

  char _appId;
  String _appName;
  bool _reInit = false;

#if ENABLE_STATUS_EVTSRC
  WiFiClient _statusEvtSrcClient[STATUS_EVTSRC_MAX_CLIENTS];
#if ENABLE_STATUS_EVTSRC_KEEPALIVE
  bool _needStatusEvtSrcKeepAlive = false;
  Ticker _statusEvtSrcKeepAliveTicker;
#endif
#endif

#if ENABLE_STATUS_EVTSRC
  void statusEventSourceHandler(ESP8266WebServer &server);
  void statusEventSourceBroadcast(const String &message, const String &eventType = "message");
#if ENABLE_STATUS_EVTSRC_KEEPALIVE
  void statusEventSourceKeepAlive();
#endif
#endif

  // already built methods
  bool saveConfig();
  bool loadConfig();

  // specialization required from the application
  virtual void setConfigDefaultValues() = 0;
  virtual void parseConfigJSON(DynamicJsonDocument &doc) = 0;
  virtual bool parseConfigWebRequest(ESP8266WebServer &server) = 0;
  virtual String generateConfigJSON(bool forSaveFile = false) = 0;
  virtual String generateStatusJSON() = 0;
  virtual bool appInit(bool reInit = false) = 0;
  virtual const PROGMEM char *getHTMLContent(WebPageForPlaceHolder wp) = 0;
  virtual size_t getHTMLContentSize(WebPageForPlaceHolder wp) = 0;
  virtual void appInitWebServer(ESP8266WebServer &server, bool &shouldReboot, bool &pauseApplication) = 0;
  virtual void appRun() = 0;

public:
  // already built methods
  Application(char appId, String appName)
  {
    _appId = appId;
    _appName = appName;
  }
  void init(bool skipExistingConfig);
  void initWebServer(ESP8266WebServer &server, bool &shouldReboot, bool &pauseApplication);
  void run();
};

#endif
