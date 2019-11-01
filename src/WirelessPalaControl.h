#ifndef WirelessPalaControl_h
#define WirelessPalaControl_h

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "Main.h"
#include "base\Utils.h"
#include "base\Application.h"

const char appDataPredefPassword[] PROGMEM = "ewcXoCt4HHjZUvY1";

#include "data\status1.html.gz.h"
#include "data\config1.html.gz.h"

#include <PolledTimeout.h>
#include <SoftwareSerial.h>
#include <Palazzetti.h>

class WebPalaControl : public Application
{
private:
  //TODO : Declare configuration properies

  Palazzetti m_Pala;

  int myOpenSerial(uint32_t baudrate);
  void myCloseSerial();
  int mySelectSerial(unsigned long timeout);
  size_t myReadSerial(void *buf, size_t count);
  size_t myWriteSerial(const void *buf, size_t count);
  int myDrainSerial();
  int myFlushSerial();
  void myUSleep(unsigned long usecond);

  void SetConfigDefaultValues();
  void ParseConfigJSON(DynamicJsonDocument &doc);
  bool ParseConfigWebRequest(AsyncWebServerRequest *request);
  String GenerateConfigJSON(bool forSaveFile);
  String GenerateStatusJSON();
  bool AppInit(bool reInit);
  const uint8_t *GetHTMLContent(WebPageForPlaceHolder wp);
  size_t GetHTMLContentSize(WebPageForPlaceHolder wp);
  void AppInitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication);
  void AppRun();

public:
  WebPalaControl(char appId, String fileName);
};

#endif
