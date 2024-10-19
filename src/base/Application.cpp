#include "Application.h"

Application *Application::_applicationList[3] = {nullptr, nullptr, nullptr};

bool Application::saveConfig()
{
  File configFile = LittleFS.open(String('/') + _appName + F(".json"), "w");
  if (!configFile)
  {
#ifdef LOG_SERIAL
    LOG_SERIAL.println(F("Failed to open config file for writing"));
#endif
    return false;
  }

  configFile.print(generateConfigJSON(true));
  configFile.close();
  return true;
}

bool Application::loadConfig()
{
  // special exception for Core, there is no Core.json file to Load
  if (_appId == '0')
    return true;

  bool result = false;
  File configFile = LittleFS.open(String('/') + _appName + F(".json"), "r");
  if (configFile)
  {

    JsonDocument jsonDoc;

    DeserializationError deserializeJsonError = deserializeJson(jsonDoc, configFile);

    // if deserialization failed, then log error and save current config (default values)
    if (deserializeJsonError)
    {

#ifdef LOG_SERIAL
      LOG_SERIAL.print(F("deserializeJson() failed : "));
      LOG_SERIAL.println(deserializeJsonError.c_str());
#endif

      saveConfig();
    }
    else
    { // otherwise pass it to application
      result = parseConfigJSON(jsonDoc);
    }
    configFile.close();
  }

  return result;
}

String Application::getStatusJSON()
{
  return generateStatusJSON();
}

void Application::init(bool skipExistingConfig)
{
  bool result = true;

#ifdef LOG_SERIAL
  LOG_SERIAL.print(F("Start "));
  LOG_SERIAL.print(_appName);
  LOG_SERIAL.print(F(" : "));
#endif

  setConfigDefaultValues();

  if (!skipExistingConfig)
    result = loadConfig();

  // Execute specific Application Init Code
  result = appInit() && result;

#ifdef LOG_SERIAL
  if (result)
    LOG_SERIAL.println(F("OK"));
  else
    LOG_SERIAL.println(F("FAILED"));
#endif
}

void Application::initWebServer(WebServer &server, bool &shouldReboot, bool &pauseApplication)
{
  char url[16];

  // HTML Status handler
  sprintf_P(url, PSTR("/status%c.html"), _appId);
  server.on(url, HTTP_GET, [this, &server]()
            {
    SERVER_KEEPALIVE_FALSE()
    server.sendHeader(F("Content-Encoding"), F("gzip"));
    server.send_P(200, PSTR("text/html"), getHTMLContent(status), getHTMLContentSize(status)); });

  // HTML Config handler
  sprintf_P(url, PSTR("/config%c.html"), _appId);
  server.on(url, HTTP_GET, [this, &server]()
            {
    SERVER_KEEPALIVE_FALSE()
    server.sendHeader(F("Content-Encoding"), F("gzip"));
    server.send_P(200, PSTR("text/html"), getHTMLContent(config), getHTMLContentSize(config)); });

  // JSON Status handler
  sprintf_P(url, PSTR("/gs%c"), _appId);
  server.on(url, HTTP_GET, [this, &server]()
            {
    SERVER_KEEPALIVE_FALSE()
    server.sendHeader(F("Cache-Control"), F("no-cache"));
    server.send(200, F("text/json"), generateStatusJSON()); });

  // JSON Config handler
  sprintf_P(url, PSTR("/gc%c"), _appId);
  server.on(url, HTTP_GET, [this, &server]()
            {
    SERVER_KEEPALIVE_FALSE()
    server.sendHeader(F("Cache-Control"), F("no-cache"));
    server.send(200, F("text/json"), generateConfigJSON()); });

  sprintf_P(url, PSTR("/sc%c"), _appId);
  server.on(url, HTTP_POST, [this, &server]()
            {
    // All responses have keep-alive set to false
    SERVER_KEEPALIVE_FALSE()

    // config json are received in POST body (arg("plain"))

    // Deserialize it
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error)
    {
      server.send(400, F("text/html"), F("Malformed JSON"));
      return;
    }

    // Parse it using the application method
    if (!parseConfigJSON(doc, true)){
      server.send(400, F("text/html"), F("Invalid Configuration"));
      return;
    }

    // Save it
    if (!saveConfig()){
      server.send(500, F("text/html"), F("Configuration hasn't been saved"));
      return;
    }

    //Everything went fine, Send client answer
    server.send(200);
    _reInit = true; });

  // Execute Specific Application Web Server initialization
  appInitWebServer(server, shouldReboot, pauseApplication);
}

void Application::run()
{
  if (_reInit)
  {
#ifdef LOG_SERIAL
    LOG_SERIAL.print(F("ReStart "));
    LOG_SERIAL.print(_appName);
    LOG_SERIAL.print(F(" : "));
#endif

    if (appInit(true))
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

  appRun();
}