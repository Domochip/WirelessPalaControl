#include "Core.h"
#include <EEPROM.h>
#include <SPIFFSEditor.h>
#include "..\Main.h" //for VERSION define
#include "Version.h" //for BASE_VERSION define

#include "data\index.html.gz.h"
#include "data\pure-min.css.gz.h"
#include "data\side-menu.css.gz.h"
#include "data\side-menu.js.gz.h"

void Core::SetConfigDefaultValues(){};
void Core::ParseConfigJSON(DynamicJsonDocument &doc){};
bool Core::ParseConfigWebRequest(AsyncWebServerRequest *request) { return true; };
String Core::GenerateConfigJSON(bool clearPassword = false) { return String(); };
String Core::GenerateStatusJSON()
{
  String gs('{');

  char sn[9];
  snprintf_P(sn, sizeof(sn), PSTR("%08x"), ESP.getChipId());
  unsigned long minutes = millis() / 60000;

  gs = gs + F("\"sn\":\"") + sn + '"';
  gs = gs + F(",\"b\":\"") + BASE_VERSION + '/' + VERSION + '"';
  gs = gs + F(",\"u\":\"") + (byte)(minutes / 1440) + 'd' + (byte)(minutes / 60 % 24) + 'h' + (byte)(minutes % 60) + 'm' + '"';
  gs = gs + F(",\"f\":") + ESP.getFreeHeap();
  gs = gs + F(",\"fcrs\":") + ESP.getFlashChipRealSize();

  gs = gs + '}';

  return gs;
};
bool Core::AppInit(bool reInit = false) { return true; };
const uint8_t *Core::GetHTMLContent(WebPageForPlaceHolder wp)
{
  switch (wp)
  {
  case status:
    return (const uint8_t *)status0htmlgz;
    break;
  case config:
    return (const uint8_t *)config0htmlgz;
    break;
  case fw:
    return (const uint8_t *)fw0htmlgz;
    break;
  case discover:
    return (const uint8_t *)discover0htmlgz;
    break;
  };
  return nullptr;
};
//and his Size
size_t Core::GetHTMLContentSize(WebPageForPlaceHolder wp)
{
  switch (wp)
  {
  case status:
    return sizeof(status0htmlgz);
    break;
  case config:
    return sizeof(config0htmlgz);
    break;
  case fw:
    return sizeof(fw0htmlgz);
    break;
  case discover:
    return sizeof(discover0htmlgz);
    break;
  };
  return 0;
};
void Core::AppInitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication)
{
  //root is index
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), (const uint8_t *)indexhtmlgz, sizeof(indexhtmlgz));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  //sn url is a way to find module on network
  char discoURL[10];
  sprintf_P(discoURL, PSTR("/%08x"), ESP.getChipId());
  server.on(discoURL, HTTP_GET, [](AsyncWebServerRequest *request) {
    char chipID[9];
    sprintf_P(chipID, PSTR("%08x"), ESP.getChipId());
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", chipID);
    response->addHeader("Access-Control-Allow-Origin", "*"); //allow this URL to be requested from everywhere
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  //ffffffff url is a way to find all modules on the network
  server.on("/ffffffff", HTTP_GET, [](AsyncWebServerRequest *request) {
    //answer with a JSON string containing sn, model and version numbers
    char discoJSON[128];
    sprintf_P(discoJSON, PSTR("{\"sn\":\"%08x\",\"m\":\"%s\",\"v\":\"%s\"}"), ESP.getChipId(), APPLICATION1_NAME, BASE_VERSION "/" VERSION);
    AsyncWebServerResponse *response = request->beginResponse(200, "text/json", discoJSON);
    response->addHeader("Access-Control-Allow-Origin", "*"); //allow this URL to be requested from everywhere
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  //FirmWare POST URL allows to push new firmware
  server.on("/fw", HTTP_POST, [&shouldReboot, &pauseApplication](AsyncWebServerRequest *request) {
    shouldReboot = !Update.hasError();
    if (shouldReboot) {
      AsyncWebServerResponse *response = request->beginResponse(200, F("text/html"), F("Firmware Successfully Uploaded<script>setTimeout(function(){if('referrer' in document)window.location=document.referrer;},10000);</script>"));
      response->addHeader("Connection", "close");
      request->send(response);
    }
    else {
      //Upload failed so restart to Run Application in loop
      pauseApplication = false;
      //Prepare response
      String errorMsg(Update.getError());
      errorMsg+=' ';
      switch(Update.getError()){
        case UPDATE_ERROR_WRITE:
          errorMsg=F("Flash Write Failed");
          break;
        case UPDATE_ERROR_ERASE:
          errorMsg=F("Flash Erase Failed");
          break;
        case UPDATE_ERROR_READ:
          errorMsg=F("Flash Read Failed");
          break;
        case UPDATE_ERROR_SPACE:
          errorMsg=F("Not Enough Space");
          break;
        case UPDATE_ERROR_SIZE:
          errorMsg=F("Bad Size Given");
          break;
        case UPDATE_ERROR_STREAM:
          errorMsg=F("Stream Read Timeout");
          break;
        case UPDATE_ERROR_MD5:
          errorMsg=F("MD5 Check Failed");
          break;
        case UPDATE_ERROR_FLASH_CONFIG:
          errorMsg=F("Flash config wrong");
          break;
        case UPDATE_ERROR_NEW_FLASH_CONFIG:
          errorMsg=F("New Flash config wrong");
          break;
        case UPDATE_ERROR_MAGIC_BYTE:
          errorMsg=F("Magic byte is wrong, not 0xE9");
          break;
        case UPDATE_ERROR_BOOTSTRAP:
          errorMsg=F("Invalid bootstrapping state, reset ESP8266 before updating");
          break;
        default:
          errorMsg=F("Unknown error");
          break;
      }
      AsyncWebServerResponse *response = request->beginResponse(500, F("text/html"), errorMsg);
      response->addHeader("Connection", "close");
      request->send(response);
    } }, [&pauseApplication](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
      //stop to Run Application in loop
      pauseApplication = true;
#ifdef LOG_SERIAL
      LOG_SERIAL.printf("Update Start: %s\n", filename.c_str());
#endif
      Update.runAsync(true);
      if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
#ifdef LOG_SERIAL
        Update.printError(LOG_SERIAL);
#endif
      }
    }
    if (!Update.hasError()) {
      //Feed the dog otherwise big firmware won't pass
      ESP.wdtFeed();
      if (Update.write(data, len) != len) {
#ifdef LOG_SERIAL
        Update.printError(LOG_SERIAL);
#endif
      }
    }
    if (final) {
      if (Update.end(true)) {
#ifdef LOG_SERIAL
        LOG_SERIAL.printf("Update Success: %uB\n", index + len);
      } else {
        Update.printError(LOG_SERIAL);
#endif
      }
    } });

  //reboot POST
  server.on("/rbt", HTTP_POST, [&shouldReboot](AsyncWebServerRequest *request) {
    request->send_P(200,F("text/html"),PSTR("Reboot command received<script>setTimeout(function(){if('referrer' in document)window.location=document.referrer;},30000);</script>"));
    shouldReboot = true; });

  //reboot Rescue POST
  server.on("/rbtrsc", HTTP_POST, [&shouldReboot](AsyncWebServerRequest *request) {
    request->send_P(200,F("text/html"),PSTR("Reboot in rescue command received<script>setTimeout(function(){if('referrer' in document)window.location=document.referrer;},30000);</script>"));
    //Set EEPROM for Rescue mode flag
    EEPROM.begin(4);
    EEPROM.write(0, 1);
    EEPROM.end();
    shouldReboot = true; });

  //Ressources URLs
  server.on("/pure-min.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/css"), (const uint8_t *)puremincssgz, sizeof(puremincssgz));
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "max-age=604800, public");
    request->send(response);
  });

  server.on("/side-menu.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/css"), (const uint8_t *)sidemenucssgz, sizeof(sidemenucssgz));
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "max-age=604800, public");
    request->send(response);
  });

  server.on("/side-menu.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/javascript"), (const uint8_t *)sidemenujsgz, sizeof(sidemenujsgz));
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "max-age=604800, public");
    request->send(response);
  });

  //Special Developper pages
#if DEVELOPPER_MODE
  server.addHandler(new SPIFFSEditor("TODO", "TODO"));
#endif

  //404 on not found
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404);
  });
}