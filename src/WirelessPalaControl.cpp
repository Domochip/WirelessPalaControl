#include "WirelessPalaControl.h"

//------------------------------------------
//Used to initialize configuration properties to default values
void WebPalaControl::SetConfigDefaultValues(){
    //TODO
    //property1 = 0;
    //property2 = F("test");
};
//------------------------------------------
//Parse JSON object into configuration properties
void WebPalaControl::ParseConfigJSON(DynamicJsonDocument &doc){
    //TODO
    //if (!doc["prop1"].isNull()) property1 = doc[F("prop1")];
    //if (!doc["prop2"].isNull()) strlcpy(property2, doc["prop2"], sizeof(property2));
};
//------------------------------------------
//Parse HTTP POST parameters in request into configuration properties
bool WebPalaControl::ParseConfigWebRequest(AsyncWebServerRequest *request)
{
    //TODO
    // if (!request->hasParam(F("prop1"), true))
    // {
    //     request->send(400, F("text/html"), F("prop1 missing"));
    //     return false;
    // }
    //if (request->hasParam(F("prop1"), true)) property1 = request->getParam(F("prop1"), true)->value().toInt();
    //if (request->hasParam(F("prop2"), true) && request->getParam(F("prop2"), true)->value().length() < sizeof(property2)) strcpy(property2, request->getParam(F("prop2"), true)->value().c_str());

    return true;
};
//------------------------------------------
//Generate JSON from configuration properties
String WebPalaControl::GenerateConfigJSON(bool forSaveFile = false)
{
    String gc('{');
    //TODO
    // gc = gc + F("\"p1\":") + (property1 ? true : false);
    // gc = gc + F("\"p2\":\"") + property2 + '"';

    gc += '}';

    return gc;
};
//------------------------------------------
//Generate JSON of application status
String WebPalaControl::GenerateStatusJSON()
{
    String gs('{');

    //TODO
    // gs = gs + F("\"p1\":") + (property1 ? true : false);
    // gs = gs + F(",\"p2\":\"") + property2 + '"';

    gs += '}';

    return gs;
};
//------------------------------------------
//code to execute during initialization and reinitialization of the app
bool WebPalaControl::AppInit(bool reInit)
{
    //TODO
    //if (toto.enabled) _sendTimer.setInterval(SEND_PERIOD, [this]() {this->SendTimerTick();});

    return true;
};
//------------------------------------------
//Return HTML Code to insert into Status Web page
const uint8_t* WebPalaControl::GetHTMLContent(WebPageForPlaceHolder wp){
      switch(wp){
    case status:
      return (const uint8_t*) status1htmlgz;
      break;
    case config:
      return (const uint8_t*) config1htmlgz;
      break;
    default:
      return nullptr;
      break;
  };
  return nullptr;
};
//and his Size
size_t WebPalaControl::GetHTMLContentSize(WebPageForPlaceHolder wp){
  switch(wp){
    case status:
      return sizeof(status1htmlgz);
      break;
    case config:
      return sizeof(config1htmlgz);
      break;
    default:
      return 0;
      break;
  };
  return 0;
};

//------------------------------------------
//code to register web request answer to the web server
void WebPalaControl::AppInitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication){
    //TODO
    //server.on("/getColor", HTTP_GET, [this](AsyncWebServerRequest * request) {request->send(200, F("text/html"), GetColor());});
};

//------------------------------------------
//Run for timer
void WebPalaControl::AppRun()
{
    //TODO : implement run tasks (receive from serial, run timer, etc.)
}

//------------------------------------------
//Constructor
WebPalaControl::WebPalaControl(char appId, String appName) : Application(appId, appName)
{
    //TODO : Initialize special structure or libraries in constructor
    //Note : most of the time, init is done during AppInit based on configuration
}