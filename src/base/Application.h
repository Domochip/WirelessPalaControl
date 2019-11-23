#ifndef Application_h
#define Application_h

#include "..\Main.h"
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

//Maximum size that can be allocated to Parsed JSON
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
    discover
  } WebPageForPlaceHolder;

  char _appId;
  String _appName;
  bool _reInit = false;

#if ENABLE_STATUS_EVENTSOURCE
  AsyncEventSource _statusEventSource; //initialized during Constructor
#endif

  //already built methods
  bool saveConfig();
  bool loadConfig();

  //specialization required from the application
  virtual void setConfigDefaultValues() = 0;
  virtual void parseConfigJSON(DynamicJsonDocument &doc) = 0;
  virtual bool parseConfigWebRequest(AsyncWebServerRequest *request) = 0;
  virtual String generateConfigJSON(bool forSaveFile = false) = 0;
  virtual String generateStatusJSON() = 0;
  virtual bool appInit(bool reInit = false) = 0;
  virtual const uint8_t *getHTMLContent(WebPageForPlaceHolder wp) = 0;
  virtual size_t getHTMLContentSize(WebPageForPlaceHolder wp) = 0;
  virtual void appInitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication) = 0;
  virtual void appRun() = 0;

public:
  //already built methods
#if ENABLE_STATUS_EVENTSOURCE
  Application(char appId, String appName) : _statusEventSource(String(F("/statusEvt")) + appId)
#else
  Application(char appId, String appName)
#endif
  {
    _appId = appId;
    _appName = appName;
  }
  void init(bool skipExistingConfig);
  void initWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication);
  void run();
};

#endif