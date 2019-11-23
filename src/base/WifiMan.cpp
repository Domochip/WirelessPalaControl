#include "WifiMan.h"

void WifiMan::enableAP(bool force = false)
{
  if (!(WiFi.getMode() & WIFI_AP) || force)
  {
    WiFi.enableAP(true);
    WiFi.softAP(_apSsid, DEFAULT_AP_PSK, _apChannel);
  }
}

void WifiMan::refreshWiFi()
{
  if (ssid[0]) //if STA configured
  {
    if (!WiFi.isConnected() || WiFi.SSID() != ssid || WiFi.psk() != password)
    {
      enableAP();
#ifdef LOG_SERIAL
      LOG_SERIAL.print(F("Connect"));
#endif
      WiFi.begin(ssid, password);
      WiFi.config(ip, gw, mask, dns1, dns2);

      //Wait _reconnectDuration for connection
      for (int i = 0; i < (((uint16_t)_reconnectDuration) * 10) && !WiFi.isConnected(); i++)
      {
        if ((i % 10) == 0)
        {
#ifdef LOG_SERIAL
          LOG_SERIAL.print(".");
#endif
        }
        delay(100);
      }

      //if connection is successfull
      if (WiFi.isConnected())
      {
        WiFi.enableAP(false); //disable AP
#ifdef STATUS_LED_GOOD
        STATUS_LED_GOOD
#endif

#ifdef LOG_SERIAL
        LOG_SERIAL.print(F("Connected ("));
        LOG_SERIAL.print(WiFi.localIP());
        LOG_SERIAL.print(F(") "));
#endif
      }
      else //connection failed
      {
        WiFi.disconnect();
#ifdef LOG_SERIAL
        LOG_SERIAL.print(F("AP not found "));
#endif
        _refreshTicker.once(_refreshPeriod, [this]() { _needRefreshWifi = true; });
      }
    }
  }
  else //else if AP is configured
  {
    _refreshTicker.detach();
    enableAP();
    WiFi.disconnect();
#ifdef STATUS_LED_GOOD
    STATUS_LED_GOOD
#endif

#ifdef LOG_SERIAL
    LOG_SERIAL.print(F(" AP mode("));
    LOG_SERIAL.print(WiFi.softAPIP());
    LOG_SERIAL.print(F(") "));
#endif
  }
}

void WifiMan::setConfigDefaultValues()
{
  ssid[0] = 0;
  password[0] = 0;
  hostname[0] = 0;
  ip = 0;
  gw = 0;
  mask = 0;
  dns1 = 0;
  dns2 = 0;
}

void WifiMan::parseConfigJSON(DynamicJsonDocument &doc)
{
  if (!doc["s"].isNull())
    strlcpy(ssid, doc["s"], sizeof(ssid));

  if (!doc["p"].isNull())
    strlcpy(password, doc["p"], sizeof(password));

  if (!doc["h"].isNull())
    strlcpy(hostname, doc["h"], sizeof(hostname));

  if (!doc["ip"].isNull())
    ip = doc["ip"];
  if (!doc["gw"].isNull())
    gw = doc["gw"];
  if (!doc["mask"].isNull())
    mask = doc["mask"];
  if (!doc["dns1"].isNull())
    dns1 = doc["dns1"];
  if (!doc["dns2"].isNull())
    dns2 = doc["dns2"];
}

bool WifiMan::parseConfigWebRequest(AsyncWebServerRequest *request)
{

  //basic control
  if (!request->hasParam(F("s"), true))
  {
    request->send(400, F("text/html"), F("SSID missing"));
    return false;
  }

  char tempPassword[64 + 1] = {0};

  if (request->hasParam(F("s"), true) && request->getParam(F("s"), true)->value().length() < sizeof(ssid))
    strcpy(ssid, request->getParam(F("s"), true)->value().c_str());

  if (request->hasParam(F("p"), true) && request->getParam(F("p"), true)->value().length() < sizeof(tempPassword))
    strcpy(tempPassword, request->getParam(F("p"), true)->value().c_str());
  if (request->hasParam(F("h"), true) && request->getParam(F("h"), true)->value().length() < sizeof(hostname))
    strcpy(hostname, request->getParam(F("h"), true)->value().c_str());

  IPAddress ipParser;
  if (request->hasParam(F("ip"), true))
  {
    if (ipParser.fromString(request->getParam(F("ip"), true)->value()))
      ip = static_cast<uint32_t>(ipParser);
    else
      ip = 0;
  }
  if (request->hasParam(F("gw"), true))
  {
    if (ipParser.fromString(request->getParam(F("gw"), true)->value()))
      gw = static_cast<uint32_t>(ipParser);
    else
      gw = 0;
  }
  if (request->hasParam(F("mask"), true))
  {
    if (ipParser.fromString(request->getParam(F("mask"), true)->value()))
      mask = static_cast<uint32_t>(ipParser);
    else
      mask = 0;
  }
  if (request->hasParam(F("dns1"), true))
  {
    if (ipParser.fromString(request->getParam(F("dns1"), true)->value()))
      dns1 = static_cast<uint32_t>(ipParser);
    else
      dns1 = 0;
  }
  if (request->hasParam(F("dns2"), true))
  {
    if (ipParser.fromString(request->getParam(F("dns2"), true)->value()))
      dns2 = static_cast<uint32_t>(ipParser);
    else
      dns2 = 0;
  }

  //check for previous password ssid (there is a predefined special password that mean to keep already saved one)
  if (strcmp_P(tempPassword, predefPassword))
    strcpy(password, tempPassword);

  return true;
}
String WifiMan::generateConfigJSON(bool forSaveFile = false)
{

  String gc('{');
  gc = gc + F("\"s\":\"") + ssid + '"';

  if (forSaveFile)
    gc = gc + F(",\"p\":\"") + password + '"';
  else
    //there is a predefined special password (mean to keep already saved one)
    gc = gc + F(",\"p\":\"") + (__FlashStringHelper *)predefPassword + '"';
  gc = gc + F(",\"h\":\"") + hostname + '"';

  if (forSaveFile)
  {
    gc = gc + F(",\"ip\":") + ip;
    gc = gc + F(",\"gw\":") + gw;
    gc = gc + F(",\"mask\":") + mask;
    gc = gc + F(",\"dns1\":") + dns1;
    gc = gc + F(",\"dns2\":") + dns2;
  }
  else
  {
    gc = gc + F(",\"staticip\":") + (ip ? true : false);
    if (ip)
      gc = gc + F(",\"ip\":\"") + IPAddress(ip).toString() + '"';
    if (gw)
      gc = gc + F(",\"gw\":\"") + IPAddress(gw).toString() + '"';
    else
      gc = gc + F(",\"gw\":\"0.0.0.0\"");
    if (mask)
      gc = gc + F(",\"mask\":\"") + IPAddress(mask).toString() + '"';
    else
      gc = gc + F(",\"mask\":\"0.0.0.0\"");
    if (dns1)
      gc = gc + F(",\"dns1\":\"") + IPAddress(dns1).toString() + '"';
    if (dns2)
      gc = gc + F(",\"dns2\":\"") + IPAddress(dns2).toString() + '"';
  }

  gc = gc + '}';

  return gc;
}

String WifiMan::generateStatusJSON()
{

  String gs('{');
  if ((WiFi.getMode() & WIFI_AP))
  {
    gs = gs + F("\"ap\":\"on\"");
    gs = gs + F(",\"ai\":\"") + WiFi.softAPIP().toString().c_str() + '"';
  }
  else
  {
    gs = gs + F("\"ap\":\"off\"");
    gs = gs + F(",\"ai\":\"-\"");
  }

  gs = gs + F(",\"sta\":\"") + (ssid[0] ? F("on") : F("off")) + '"';
  gs = gs + F(",\"stai\":\"") + (ssid[0] ? (WiFi.isConnected() ? ((WiFi.localIP().toString() + ' ' + (ip ? F("Static IP") : F("DHCP"))).c_str()) : "Not Connected") : "-") + '"';

  gs = gs + F(",\"mac\":\"") + WiFi.macAddress() + '"';

  gs = gs + '}';

  return gs;
}

bool WifiMan::appInit(bool reInit = false)
{
  if (!reInit)
  {
    //build "unique" AP name (DEFAULT_AP_SSID + 4 last digit of ChipId)
    _apSsid[0] = 0;
    strcpy(_apSsid, DEFAULT_AP_SSID);
    uint16 id = ESP.getChipId() & 0xFFFF;
    byte endOfSsid = strlen(_apSsid);
    byte num = (id & 0xF000) / 0x1000;
    _apSsid[endOfSsid] = num + ((num <= 9) ? 0x30 : 0x37);
    num = (id & 0xF00) / 0x100;
    _apSsid[endOfSsid + 1] = num + ((num <= 9) ? 0x30 : 0x37);
    num = (id & 0xF0) / 0x10;
    _apSsid[endOfSsid + 2] = num + ((num <= 9) ? 0x30 : 0x37);
    num = id & 0xF;
    _apSsid[endOfSsid + 3] = num + ((num <= 9) ? 0x30 : 0x37);
    _apSsid[endOfSsid + 4] = 0;
  }

  //make changes saved to flash
  WiFi.persistent(true);

  //Enable AP at start
  enableAP(true);

  //Stop RefreshWiFi and disconnect before WiFi operations -----
  _refreshTicker.detach();
  WiFi.disconnect();

  // scan networks to search for best free channel
  int n = WiFi.scanNetworks();

#ifdef LOG_SERIAL
  LOG_SERIAL.print(n);
  LOG_SERIAL.print(F("N-CH"));
#endif
  if (n)
  {
    while (_apChannel < 12)
    {
      int i = 0;
      while (i < n && WiFi.channel(i) != _apChannel)
        i++;
      if (i == n)
        break;
      _apChannel++;
    }
  }
#ifdef LOG_SERIAL
  LOG_SERIAL.print(_apChannel);
  LOG_SERIAL.print(' ');
#endif

  //Configure handlers
  if (!reInit)
  {
    _discoEventHandler = WiFi.onStationModeDisconnected([this](const WiFiEventStationModeDisconnected &evt) {
      if (!(WiFi.getMode() & WIFI_AP) && ssid[0])
      {
        //stop reconnection
        WiFi.disconnect();
#ifdef LOG_SERIAL
        LOG_SERIAL.println(F("Wifi disconnected"));
#endif
        //call RefreshWifi shortly
        _needRefreshWifi = true;
      }
#ifdef STATUS_LED_WARNING
      STATUS_LED_WARNING
#endif
    });

    //if station connect to softAP
    _staConnectedHandler = WiFi.onSoftAPModeStationConnected([this](const WiFiEventSoftAPModeStationConnected &evt) {
      //flag it in _stationConnectedToSoftAP
      _stationConnectedToSoftAP = true;
    });
    //if station disconnect of the softAP
    _staDisconnectedHandler = WiFi.onSoftAPModeStationDisconnected([this](const WiFiEventSoftAPModeStationDisconnected &evt) {
      //check if a station left
      _stationConnectedToSoftAP = WiFi.softAPgetStationNum();
    });
  }

  //Set hostname
  WiFi.hostname(hostname);

  //Call RefreshWiFi to initiate configuration
  refreshWiFi();

  //right config so no need to touch again flash
  WiFi.persistent(false);

  return (ssid[0] ? WiFi.isConnected() : true);
};

const uint8_t *WifiMan::getHTMLContent(WebPageForPlaceHolder wp)
{
  switch (wp)
  {
  case status:
    return (const uint8_t *)statuswhtmlgz;
    break;
  case config:
    return (const uint8_t *)configwhtmlgz;
    break;
  default:
    return nullptr;
    break;
  };
  return nullptr;
};
size_t WifiMan::getHTMLContentSize(WebPageForPlaceHolder wp)
{
  switch (wp)
  {
  case status:
    return sizeof(statuswhtmlgz);
    break;
  case config:
    return sizeof(configwhtmlgz);
    break;
  default:
    return 0;
    break;
  };
  return 0;
};

void WifiMan::appInitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication)
{

  server.on("/wnl", HTTP_GET, [this](AsyncWebServerRequest *request) {
    int8_t n = WiFi.scanComplete();
    if (n == -2)
    {
      AsyncWebServerResponse *response = request->beginResponse(200, F("text/json"), F("{\"r\":-2,\"wnl\":[]}"));
      response->addHeader("Cache-Control", "no-cache");
      request->send(response);
      WiFi.scanNetworks(true);
    }
    else if (n == -1)
    {
      AsyncWebServerResponse *response = request->beginResponse(200, F("text/json"), F("{\"r\":-1,\"wnl\":[]}"));
      response->addHeader("Cache-Control", "no-cache");
      request->send(response);
    }
    else
    {
      String networksJSON(F("{\"r\":"));
      networksJSON = networksJSON + n + F(",\"wnl\":[");
      for (byte i = 0; i < n; i++)
      {
        networksJSON = networksJSON + '"' + WiFi.SSID(i) + '"';
        if (i != (n - 1))
          networksJSON += ',';
      }
      networksJSON += F("]}");
      AsyncWebServerResponse *response = request->beginResponse(200, F("text/json"), networksJSON);
      response->addHeader("Cache-Control", "no-cache");
      request->send(response);
      WiFi.scanDelete();
    }
  });
}

void WifiMan::appRun()
{
  //if refreshWifi is required and no client is connected to the softAP
  if (_needRefreshWifi && !_stationConnectedToSoftAP)
  {
    _needRefreshWifi = false;
    refreshWiFi();
  }
};