#ifndef Core_h
#define Core_h

#include "..\Main.h"

#include "data\status0.html.gz.h"
#include "data\config0.html.gz.h"
#include "data\fw0.html.gz.h"
#include "data\discover0.html.gz.h"

class Core : public Application
{
private:
  void SetConfigDefaultValues();
  void ParseConfigJSON(DynamicJsonDocument &doc);
  bool ParseConfigWebRequest(AsyncWebServerRequest *request);
  String GenerateConfigJSON(bool clearPassword);
  String GenerateStatusJSON();
  bool AppInit(bool reInit);
  const uint8_t* GetHTMLContent(WebPageForPlaceHolder wp);
  size_t GetHTMLContentSize(WebPageForPlaceHolder wp);
  void AppInitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication);
  void AppRun(){};

public:
  Core(char appId, String fileName) : Application(appId, fileName) {}
};

#endif