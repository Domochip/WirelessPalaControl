#include "Application.h"

bool Application::SaveConfig()
{
  File configFile = SPIFFS.open(String('/') + _appName + F(".json"), "w");
  if (!configFile)
  {
#ifdef LOG_SERIAL
    LOG_SERIAL.println(F("Failed to open config file for writing"));
#endif
    return false;
  }

  configFile.print(GenerateConfigJSON(true));
  configFile.close();
  return true;
}

bool Application::LoadConfig()
{
  //special exception for Core, there is no Core.json file to Load
  if (_appId == '0')
    return true;

  bool result = false;
  File configFile = SPIFFS.open(String('/') + _appName + F(".json"), "r");
  if (configFile)
  {

    int memoryAllocation = JSON_DOC_MEM_STEP;

    while (memoryAllocation <= JSON_DOC_MAX_MEM_SIZE)
    {
      DynamicJsonDocument jsonDoc(memoryAllocation);

      configFile.seek(0);

      auto deserializeJsonError = deserializeJson(jsonDoc, configFile);
      //if parsing OK, pass it to application then stop loop
      if (deserializeJsonError.code() == DeserializationError::Ok)
      {
        ParseConfigJSON(jsonDoc);
        result = true;
        break;
      }
      //if parsing result is not a NoMemory, there is a problem in JSON
      if (deserializeJsonError.code() != DeserializationError::NoMemory)
      {
#ifdef LOG_SERIAL
        LOG_SERIAL.print(F("deserializeJson() failed : "));
        LOG_SERIAL.println(deserializeJsonError.c_str());
#endif
        break;
      }

      //there, we need to increase memorySize and loop
      memoryAllocation += JSON_DOC_MEM_STEP;
    }

    configFile.close();
  }

  //if loading failed, then run a Save to write a good one
  if (!result)
    SaveConfig();

  return result;
}

void Application::Init(bool skipExistingConfig)
{
  bool result = true;

#ifdef LOG_SERIAL
  LOG_SERIAL.print(F("Start "));
  LOG_SERIAL.print(_appName);
  LOG_SERIAL.print(F(" : "));
#endif

  SetConfigDefaultValues();

  if (!skipExistingConfig)
    result = LoadConfig();

  //Execute specific Application Init Code
  result = AppInit() && result;

#ifdef LOG_SERIAL
  if (result)
    LOG_SERIAL.println(F("OK"));
  else
    LOG_SERIAL.println(F("FAILED"));
#endif
}

void Application::InitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication)
{
  char url[16];

  //HTML Status handler
  sprintf_P(url, PSTR("/status%c.html"), _appId);
  server.on(url, HTTP_GET, [this](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), GetHTMLContent(status), GetHTMLContentSize(status));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  //HTML Config handler
  sprintf_P(url, PSTR("/config%c.html"), _appId);
  server.on(url, HTTP_GET, [this](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), GetHTMLContent(config), GetHTMLContentSize(config));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  //HTML fw handler
  sprintf_P(url, PSTR("/fw%c.html"), _appId);
  server.on(url, HTTP_GET, [this](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), GetHTMLContent(fw), GetHTMLContentSize(fw));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  //HTML discover handler
  sprintf_P(url, PSTR("/discover%c.html"), _appId);
  server.on(url, HTTP_GET, [this](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), GetHTMLContent(discover), GetHTMLContentSize(discover));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  //JSON Status handler
  sprintf_P(url, PSTR("/gs%c"), _appId);
  server.on(url, HTTP_GET, [this](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, F("text/json"), GenerateStatusJSON());
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  //JSON Config handler
  sprintf_P(url, PSTR("/gc%c"), _appId);
  server.on(url, HTTP_GET, [this](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, F("text/json"), GenerateConfigJSON());
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  sprintf_P(url, PSTR("/sc%c"), _appId);
  server.on(url, HTTP_POST, [this](AsyncWebServerRequest *request) {
    if (!ParseConfigWebRequest(request))
      return;

    //then save
    bool result = SaveConfig();

    //Send client answer
    if (result)
    {
      request->send(200);
      _reInit = true;
    }
    else
      request->send(500, F("text/html"), F("Configuration hasn't been saved"));
  });

  //Execute Specific Application Web Server initialization
  AppInitWebServer(server, shouldReboot, pauseApplication);
}

void Application::Run()
{
  if (_reInit)
  {
#ifdef LOG_SERIAL
    LOG_SERIAL.print(F("ReStart "));
    LOG_SERIAL.print(_appName);
    LOG_SERIAL.print(F(" : "));
#endif

    if (AppInit(true))
    {
#ifdef LOG_SERIAL
      LOG_SERIAL.println(F("OK"));
    }
    else
    {
      LOG_SERIAL.println(F("FAILED"));
#endif
    }

    _reInit = false;
  }

  AppRun();
}