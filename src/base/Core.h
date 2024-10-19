#ifndef Core_h
#define Core_h

#include "../Main.h"
#include "Application.h"
#ifdef ESP32
#include <Update.h>
#endif

#include "data/status0.html.gz.h"
#include "data/config0.html.gz.h"
#include "data/fw.html.gz.h"
#include "data/discover.html.gz.h"

class Core : public Application
{
private:
  void setConfigDefaultValues();
  bool parseConfigJSON(JsonDocument &doc, bool fromWebPage);
  String generateConfigJSON(bool clearPassword);
  String generateStatusJSON();
  bool appInit(bool reInit);
  const PROGMEM char *getHTMLContent(WebPageForPlaceHolder wp);
  size_t getHTMLContentSize(WebPageForPlaceHolder wp);
  void appInitWebServer(WebServer &server, bool &shouldReboot, bool &pauseApplication);
  void appRun() {};

public:
  Core(char appId, String appName) : Application(appId, appName) { _applicationList[Application::Applications::Core] = this; }
};

#endif