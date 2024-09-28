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
  if (ssid[0]) // if STA configured
  {
    if (!WiFi.isConnected() || WiFi.SSID() != ssid || WiFi.psk() != password)
    {
      enableAP();
#ifdef LOG_SERIAL
      LOG_SERIAL.print(F("Connect"));
#endif
      WiFi.begin(ssid, password);
      WiFi.config(ip, gw, mask, dns1, dns2);

      // Wait _reconnectDuration for connection
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

      // if connection is successfull
      if (WiFi.isConnected())
      {
        WiFi.enableAP(false); // disable AP
#ifdef STATUS_LED_GOOD
        STATUS_LED_GOOD
#endif

#ifdef LOG_SERIAL
        LOG_SERIAL.print(F("Connected ("));
        LOG_SERIAL.print(WiFi.localIP());
        LOG_SERIAL.print(F(") "));
#endif
      }
      else // connection failed
      {
        WiFi.disconnect();
#ifdef LOG_SERIAL
        LOG_SERIAL.print(F("AP not found "));
#endif
#ifdef ESP8266
        _refreshTicker.once(_refreshPeriod, [this]()
                            { _needRefreshWifi = true; });
#else
        _refreshTicker.once<typeof this>(_refreshPeriod * 1000, [](typeof this wifiMan)
                                         { wifiMan->_needRefreshWifi = true; }, this);
#endif
      }
    }
  }
  else // else if AP is configured
  {
    _refreshTicker.detach();
    enableAP();
    WiFi.disconnect();
#ifdef STATUS_LED_GOOD
    STATUS_LED_GOOD
#endif

#ifdef LOG_SERIAL
    LOG_SERIAL.print(F(" AP mode("));
    LOG_SERIAL.print(_apSsid);
    LOG_SERIAL.print(F(" - "));
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

void WifiMan::parseConfigJSON(JsonDocument &doc, bool fromWebPage = false)
{
  JsonVariant jv;
  char tempPassword[64 + 1] = {0};

  if ((jv = doc["s"]).is<const char *>() && strlen(jv) < sizeof(ssid))
    strcpy(ssid, jv);

  if ((jv = doc["p"]).is<const char *>() && strlen(jv) < sizeof(tempPassword))
    strcpy(tempPassword, jv);

  // if not from web page or password is not the predefined one
  if (!fromWebPage || strcmp_P(tempPassword, predefPassword))
    strcpy(password, tempPassword);

  if ((jv = doc["h"]).is<const char *>() && strlen(jv) < sizeof(hostname))
    strcpy(hostname, jv);

  IPAddress ipParser;
  if ((jv = doc["ip"]).is<const char *>() && ipParser.fromString(jv.as<const char *>()))
    ip = static_cast<uint32_t>(ipParser);
  else
    ip = 0;

  if ((jv = doc["gw"]).is<const char *>() && ipParser.fromString(jv.as<const char *>()))
    gw = static_cast<uint32_t>(ipParser);
  else
    gw = 0;

  if ((jv = doc["mask"]).is<const char *>() && ipParser.fromString(jv.as<const char *>()))
    mask = static_cast<uint32_t>(ipParser);
  else
    mask = 0;

  if ((jv = doc["dns1"]).is<const char *>() && ipParser.fromString(jv.as<const char *>()))
    dns1 = static_cast<uint32_t>(ipParser);
  else
    dns1 = 0;

  if ((jv = doc["dns2"]).is<const char *>() && ipParser.fromString(jv.as<const char *>()))
    dns2 = static_cast<uint32_t>(ipParser);
  else
    dns2 = 0;
}

bool WifiMan::parseConfigWebRequest(WebServer &server)
{
  // config json is received in POST body (arg("plain"))

  // parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error)
  {
    SERVER_KEEPALIVE_FALSE()
    server.send(400, F("text/html"), F("Invalid JSON"));
    return false;
  }

  // basic control
  if (!doc["s"].is<const char *>())
  {
    SERVER_KEEPALIVE_FALSE()
    server.send(400, F("text/html"), F("SSID missing"));
    return false;
  }

  parseConfigJSON(doc, true);

  return true;
}

String WifiMan::generateConfigJSON(bool forSaveFile = false)
{
  JsonDocument doc;

  doc["s"] = ssid;

  if (forSaveFile)
    doc["p"] = password;
  else
    // there is a predefined special password (mean to keep already saved one)
    doc["p"] = (const __FlashStringHelper *)predefPassword;

  doc["h"] = hostname;

  doc["staticip"] = (ip ? 1 : 0);
  if (ip)
    doc["ip"] = IPAddress(ip).toString();
  if (gw)
    doc["gw"] = IPAddress(gw).toString();
  else
    doc["gw"] = F("0.0.0.0");
  if (mask)
    doc["mask"] = IPAddress(mask).toString();
  else
    doc["mask"] = F("0.0.0.0");
  if (dns1)
    doc["dns1"] = IPAddress(dns1).toString();
  if (dns2)
    doc["dns2"] = IPAddress(dns2).toString();

  String gc;
  serializeJson(doc, gc);

  return gc;
}

String WifiMan::generateStatusJSON()
{
  JsonDocument doc;

  if ((WiFi.getMode() & WIFI_AP))
  {
    doc["ap"] = F("on");
    doc["ai"] = WiFi.softAPIP().toString();
  }
  else
  {
    doc["ap"] = F("off");
    doc["ai"] = "-";
  }

  doc["sta"] = (ssid[0] ? F("on") : F("off"));
  doc["stai"] = String(ssid[0] ? (WiFi.isConnected() ? (WiFi.localIP().toString() + ' ' + (ip ? F("Static IP") : F("DHCP"))).c_str() : "Not Connected") : "-");

  doc["mac"] = WiFi.macAddress();

  String gs;
  serializeJson(doc, gs);

  return gs;
}

bool WifiMan::appInit(bool reInit = false)
{
  if (!reInit)
  {
    // build "unique" AP name (DEFAULT_AP_SSID + 4 last digit of ChipId)
    _apSsid[0] = 0;

#ifdef ESP8266
    sprintf_P(_apSsid, PSTR("%s%04X"), DEFAULT_AP_SSID, ESP.getChipId() & 0xFFFF);
#else
    sprintf_P(_apSsid, PSTR("%s%04X"), DEFAULT_AP_SSID, (uint32_t)(ESP.getEfuseMac() << 40 >> 40));
#endif
  }

  // make changes saved to flash
  WiFi.persistent(true);

  // Enable AP at start
  enableAP(true);

  // Stop RefreshWiFi and disconnect before WiFi operations -----
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

  // Configure handlers
  if (!reInit)
  {
#ifdef ESP8266
    _discoEventHandler = WiFi.onStationModeDisconnected([this](const WiFiEventStationModeDisconnected &evt)
#else
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info)
#endif
                                                        {
                                                          if (!(WiFi.getMode() & WIFI_AP) && ssid[0])
                                                          {
                                                            // stop reconnection
                                                            WiFi.disconnect();
#ifdef LOG_SERIAL
                                                            LOG_SERIAL.println(F("Wifi disconnected"));
#endif
                                                            // call RefreshWifi shortly
                                                            _needRefreshWifi = true;
                                                          }
#ifdef STATUS_LED_WARNING
                                                          STATUS_LED_WARNING
#endif
                                                        }
#ifndef ESP8266
                                                        ,
                                                        WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED
#endif
    );

    // if station connect to softAP
#ifdef ESP8266
    _staConnectedHandler = WiFi.onSoftAPModeStationConnected([this](const WiFiEventSoftAPModeStationConnected &evt)
#else
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info)
#endif
                                                             {
      //flag it in _stationConnectedToSoftAP
      _stationConnectedToSoftAP = true; }
#ifndef ESP8266
                                                             ,
                                                             WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STACONNECTED
#endif
    );

    // if station disconnect of the softAP
#ifdef ESP8266
    _staDisconnectedHandler = WiFi.onSoftAPModeStationDisconnected([this](const WiFiEventSoftAPModeStationDisconnected &evt)
#else
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info)
#endif
                                                                   {
      //check if a station left
      _stationConnectedToSoftAP = WiFi.softAPgetStationNum(); }
#ifndef ESP8266
                                                                   ,
                                                                   WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STADISCONNECTED
#endif
    );
  }

  // Set hostname
  WiFi.hostname(hostname);

  // Call RefreshWiFi to initiate configuration
  refreshWiFi();

  // right config so no need to touch again flash
  WiFi.persistent(false);

  // start MDNS
  MDNS.begin(APPLICATION1_NAME);

  return (ssid[0] ? WiFi.isConnected() : true);
}

const PROGMEM char *WifiMan::getHTMLContent(WebPageForPlaceHolder wp)
{
  switch (wp)
  {
  case status:
    return statuswhtmlgz;
    break;
  case config:
    return configwhtmlgz;
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
}

void WifiMan::appInitWebServer(WebServer &server, bool &shouldReboot, bool &pauseApplication)
{

  server.on("/wnl", HTTP_GET, [this, &server]()
            {
    // prepare response
    SERVER_KEEPALIVE_FALSE()
    server.sendHeader(F("Cache-Control"), F("no-cache"));

    int8_t n = WiFi.scanComplete();
    if (n == -2)
    {
      server.send(200, F("text/json"), F("{\"r\":-2,\"wnl\":[]}"));
      WiFi.scanNetworks(true);
    }
    else if (n == -1)
    {
      server.send(200, F("text/json"), F("{\"r\":-1,\"wnl\":[]}"));
    }
    else
    {
      JsonDocument doc;
      doc["r"] = n;
      JsonArray wnl = doc["wnl"].to<JsonArray>();
      for (byte i = 0; i < n; i++)
      {
        JsonObject wnl0 = wnl.add<JsonObject>();
        wnl0["SSID"] = WiFi.SSID(i);
        wnl0["RSSI"] = WiFi.RSSI(i);
      }
      String networksJSON;
      serializeJson(doc, networksJSON);

      server.send(200, F("text/json"), networksJSON);
      WiFi.scanDelete();
    } });
}

void WifiMan::appRun()
{
  // if refreshWifi is required and no client is connected to the softAP
  if (_needRefreshWifi && !_stationConnectedToSoftAP)
  {
    _needRefreshWifi = false;
    refreshWiFi();
  }
#ifdef ESP8266
  MDNS.update();
#endif
}