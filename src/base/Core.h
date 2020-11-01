#ifndef Core_h
#define Core_h

#include "..\Main.h"
#include "Application.h"
#ifdef ESP32
#include <Update.h>
#endif

#include "data\status0.html.gz.h"
#include "data\config0.html.gz.h"
#include "data\fw0.html.gz.h"
#include "data\discover0.html.gz.h"

class Core : public Application
{
private:
  void setConfigDefaultValues();
  void parseConfigJSON(DynamicJsonDocument &doc);
  bool parseConfigWebRequest(AsyncWebServerRequest *request);
  String generateConfigJSON(bool clearPassword);
  String generateStatusJSON();
  bool appInit(bool reInit);
  const uint8_t* getHTMLContent(WebPageForPlaceHolder wp);
  size_t getHTMLContentSize(WebPageForPlaceHolder wp);
  void appInitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication);
  void appRun(){};

public:
  Core(char appId, String fileName) : Application(appId, fileName) {}
};

#endif