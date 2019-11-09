#include "WirelessPalaControl.h"

//Serial management functions -------------
int WebPalaControl::myOpenSerial(uint32_t baudrate)
{
  Serial.begin(baudrate);
  Serial.pins(15, 13); //swap ESP8266 pins to alternative positions (D7(GPIO13)(RX)/D8(GPIO15)(TX))
  return 0;
}
void WebPalaControl::myCloseSerial() { Serial.end(); }
int WebPalaControl::mySelectSerial(unsigned long timeout)
{
  unsigned long startmillis = millis();
  esp8266::polledTimeout::periodicMs timeOut(10);
  while (!Serial.available() && (startmillis + timeout) > millis())
  {
    timeOut.reset();
    while (!timeOut)
      ;
    ESP.wdtFeed(); //feed the WDT to prevent bite
  }

  if (Serial.available())
    return 1;
  return 0;
}
size_t WebPalaControl::myReadSerial(void *buf, size_t count) { return Serial.read((char *)buf, count); }
size_t WebPalaControl::myWriteSerial(const void *buf, size_t count) { return Serial.write((const uint8_t *)buf, count); }
int WebPalaControl::myDrainSerial()
{
  Serial.flush(); //On ESP, Serial.flush() is drain
  return 0;
}
int WebPalaControl::myFlushSerial()
{
  Serial.flush();
  return 0;
}
void WebPalaControl::myUSleep(unsigned long usecond) { delayMicroseconds(usecond); }

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
  bool res = true;
  res &= m_Pala.initialize(
      std::bind(&WebPalaControl::myOpenSerial, this, std::placeholders::_1),
      std::bind(&WebPalaControl::myCloseSerial, this),
      std::bind(&WebPalaControl::mySelectSerial, this, std::placeholders::_1),
      std::bind(&WebPalaControl::myReadSerial, this, std::placeholders::_1, std::placeholders::_2),
      std::bind(&WebPalaControl::myWriteSerial, this, std::placeholders::_1, std::placeholders::_2),
      std::bind(&WebPalaControl::myDrainSerial, this),
      std::bind(&WebPalaControl::myFlushSerial, this),
      std::bind(&WebPalaControl::myUSleep, this, std::placeholders::_1));

  float setPoint;
  res &= m_Pala.getSetPoint(&setPoint);
  if (res)
    LOG_SERIAL.printf("setpoint=%.2f", setPoint);
  return res;
};
//------------------------------------------
//Return HTML Code to insert into Status Web page
const uint8_t *WebPalaControl::GetHTMLContent(WebPageForPlaceHolder wp)
{
  switch (wp)
  {
  case status:
    return (const uint8_t *)status1htmlgz;
    break;
  case config:
    return (const uint8_t *)config1htmlgz;
    break;
  default:
    return nullptr;
    break;
  };
  return nullptr;
};
//and his Size
size_t WebPalaControl::GetHTMLContentSize(WebPageForPlaceHolder wp)
{
  switch (wp)
  {
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
void WebPalaControl::AppInitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication)
{

  server.on("/cgi-bin/sendmsg.lua", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (request->hasParam(F("cmd")))
    {
      const String &cmd = request->getParam(F("cmd"))->value();

      if (cmd == F("GET ALLS"))
      {
        bool res = true;

        uint16_t STATUS;
        res &= m_Pala.getStatus(&STATUS, nullptr);

        float T1;
        res &= m_Pala.getAllTemps(&T1, nullptr, nullptr, nullptr, nullptr);

        float setPoint;
        res &= m_Pala.getSetPoint(&setPoint);

        uint16_t PQT;
        res &= m_Pala.getPelletQtUsed(&PQT);

        if (res)
        {
          DynamicJsonDocument doc(500);
          String jsonToReturn;
          JsonObject data = doc.createNestedObject(F("DATA"));
          data[F("STATUS")] = STATUS;
          data[F("T1")] = T1;
          data[F("SETP")] = setPoint;
          data[F("PQT")] = PQT;
          serializeJson(doc, jsonToReturn);

          request->send(200, F("text/json"), jsonToReturn);
          return;
        }
        else
        {
          request->send(500, F("text/html"), F("Stove communication failed"));
          return;
        }
      }

      if (cmd == F("GET TMPS"))
      {
        bool res = true;

        float T1, T2, T3, T4, T5;
        res &= m_Pala.getAllTemps(&T1, &T2, &T3, &T4, &T5);

        if (res)
        {
          DynamicJsonDocument doc(500);
          String jsonToReturn;
          JsonObject data = doc.createNestedObject(F("DATA"));
          data[F("T1")] = T1;
          data[F("T2")] = T2;
          data[F("T3")] = T3;
          data[F("T4")] = T4;
          data[F("T5")] = T5;
          serializeJson(doc, jsonToReturn);

          request->send(200, F("text/json"), jsonToReturn);
          return;
        }
        else
        {
          request->send(500, F("text/html"), F("Stove communication failed"));
          return;
        }
      }

      if (cmd == F("GET FAND"))
      {
        bool res = true;

        uint16_t F1V, F2V, F1RPM, F2L, F2LF, F3L, F4L;
        bool isF3LF4LValid;
        res &= m_Pala.getFanData(&F1V, &F2V, &F1RPM, &F2L, &F2LF, &isF3LF4LValid, &F3L, &F4L);

        if (res)
        {
          DynamicJsonDocument doc(500);
          String jsonToReturn;
          JsonObject data = doc.createNestedObject(F("DATA"));
          data[F("F1V")] = F1V;
          data[F("F2V")] = F2V;
          data[F("F1RPM")] = F1RPM;
          data[F("F2L")] = F2L;
          data[F("F2LF")] = F2LF;
          if (isF3LF4LValid)
          {
            data[F("F3L")] = F3L;
            data[F("F4L")] = F4L;
          }
          serializeJson(doc, jsonToReturn);

          request->send(200, F("text/json"), jsonToReturn);
          return;
        }
        else
        {
          request->send(500, F("text/html"), F("Stove communication failed"));
          return;
        }
      }

      if (cmd == F("GET SETP"))
      {
        bool res = true;

        float SETP;
        res &= m_Pala.getSetPoint(&SETP);

        if (res)
        {
          DynamicJsonDocument doc(100);
          String jsonToReturn;
          JsonObject data = doc.createNestedObject(F("DATA"));
          data[F("SETP")] = SETP;
          serializeJson(doc, jsonToReturn);

          request->send(200, F("text/json"), jsonToReturn);
          return;
        }
        else
        {
          request->send(500, F("text/html"), F("Stove communication failed"));
          return;
        }
      }

      if (cmd == F("GET POWR"))
      {
        bool res = true;

        byte PWR;
        float FDR;
        res &= m_Pala.getPower(&PWR, &FDR);

        if (res)
        {
          DynamicJsonDocument doc(100);
          String jsonToReturn;
          JsonObject data = doc.createNestedObject(F("DATA"));
          data[F("PWR")] = PWR;
          data[F("FDR")] = FDR;
          serializeJson(doc, jsonToReturn);

          request->send(200, F("text/json"), jsonToReturn);
          return;
        }
        else
        {
          request->send(500, F("text/html"), F("Stove communication failed"));
          return;
        }
      }

      if (cmd.startsWith(F("GET PARM ")))
      {
        bool res = true;

        String strParamNumber(cmd.substring(9));

        byte paramNumber = strParamNumber.toInt();

        if (paramNumber == 0 && strParamNumber[0] != '0')
        {
          String ret(F("Incorrect Parameter Number : "));
          ret += strParamNumber;
          request->send(400, F("text/html"), ret);
          return;
        }

        byte paramValue;
        res &= m_Pala.getParameter(paramNumber, &paramValue);

        if (res)
        {
          DynamicJsonDocument doc(100);
          String jsonToReturn;
          JsonObject data = doc.createNestedObject(F("DATA"));
          data[F("PAR")] = paramValue;
          serializeJson(doc, jsonToReturn);

          request->send(200, F("text/json"), jsonToReturn);
          return;
        }
        else
        {
          request->send(500, F("text/html"), F("Stove communication failed"));
          return;
        }
      }

      if (cmd.startsWith(F("GET HPAR ")))
      {
        bool res = true;

        String strHiddenParamNumber(cmd.substring(9));

        byte hiddenParamNumber = strHiddenParamNumber.toInt();

        if (hiddenParamNumber == 0 && strHiddenParamNumber[0] != '0')
        {
          String ret(F("Incorrect Hidden Parameter Number : "));
          ret += strHiddenParamNumber;
          request->send(400, F("text/html"), ret);
          return;
        }

        uint16_t hiddenParamValue;
        res &= m_Pala.getHiddenParameter(hiddenParamNumber, &hiddenParamValue);

        if (res)
        {
          DynamicJsonDocument doc(100);
          String jsonToReturn;
          JsonObject data = doc.createNestedObject(F("DATA"));
          data[F("HPAR")] = hiddenParamValue;
          serializeJson(doc, jsonToReturn);

          request->send(200, F("text/json"), jsonToReturn);
          return;
        }
        else
        {
          request->send(500, F("text/html"), F("Stove communication failed"));
          return;
        }
      }

      if (cmd.startsWith(F("CMD ")))
      {
        bool res = true;

        if (cmd.substring(4) == F("ON"))
          res &= m_Pala.powerOn();
        else if (cmd.substring(4) == F("OFF"))
          res &= m_Pala.powerOff();
        else
          res = false;

        if (res)
        {
          request->send(200, F("text/json"), F("{\"SUCCESS\":true}"));
          return;
        }
        else
        {
          request->send(500, F("text/html"), F("Stove communication failed"));
          return;
        }
      }

      if (cmd.startsWith(F("SET POWR ")))
      {
        bool res = true;

        byte powerLevel = cmd.substring(9).toInt();

        if (powerLevel == 0)
        {
          String ret(F("Incorrect Power value : "));
          ret += cmd;
          request->send(400, F("text/html"), ret);
          return;
        }

        res &= m_Pala.setPower(powerLevel);

        if (res)
        {
          request->send(200, F("text/json"), F("{\"SUCCESS\":true}"));
          return;
        }
        else
        {
          request->send(500, F("text/html"), F("Stove communication failed"));
          return;
        }
      }

      if (cmd.startsWith(F("SET RFAN ")))
      {
        bool res = true;

        String strRoomFanLevel(cmd.substring(9));

        byte roomFanLevel = strRoomFanLevel.toInt();

        if (roomFanLevel == 0 && strRoomFanLevel[0] != '0')
        {
          String ret(F("Incorrect Room Fan value : "));
          ret += cmd;
          request->send(400, F("text/html"), ret);
          return;
        }

        res &= m_Pala.setRoomFan(roomFanLevel);

        if (res)
        {
          request->send(200, F("text/json"), F("{\"SUCCESS\":true}"));
          return;
        }
        else
        {
          request->send(500, F("text/html"), F("Stove communication failed"));
          return;
        }
      }

      if (cmd.startsWith(F("SET SETP ")))
      {
        bool res = true;

        byte setPoint = cmd.substring(9).toInt();

        if (setPoint == 0)
        {
          String ret(F("Incorrect SetPoint value : "));
          ret += cmd;
          request->send(400, F("text/html"), ret);
          return;
        }

        res &= m_Pala.setSetpoint(setPoint);

        if (res)
        {
          request->send(200, F("text/json"), F("{\"SUCCESS\":true}"));
          return;
        }
        else
        {
          request->send(500, F("text/html"), F("Stove communication failed"));
          return;
        }
      }

      if (cmd.startsWith(F("SET STPF ")))
      {
        bool res = true;

        float setPointFloat = cmd.substring(9).toFloat();

        if (setPointFloat == 0)
        {
          String ret(F("Incorrect SetPoint Float value : "));
          ret += cmd;
          request->send(400, F("text/html"), ret);
          return;
        }

        res &= m_Pala.setSetpoint(setPointFloat);

        if (res)
        {
          request->send(200, F("text/json"), F("{\"SUCCESS\":true}"));
          return;
        }
        else
        {
          request->send(500, F("text/html"), F("Stove communication failed"));
          return;
        }
      }

      if (cmd.startsWith(F("SET PARM ")))
      {
        bool res = true;

        String strParamNumber(cmd.substring(9, cmd.indexOf(' ', 9)));
        String strParamValue(cmd.substring(cmd.indexOf(' ', 9) + 1));

        byte paramNumber = strParamNumber.toInt();

        if (paramNumber == 0 && strParamNumber[0] != '0')
        {
          String ret(F("Incorrect Parameter Number : "));
          ret += strParamNumber;
          request->send(400, F("text/html"), ret);
          return;
        }

        byte paramValue = strParamValue.toInt();

        if (paramValue == 0 && strParamValue[0] != '0')
        {
          String ret(F("Incorrect Parameter Value : "));
          ret += strParamValue;
          request->send(400, F("text/html"), ret);
          return;
        }

        res &= m_Pala.setParameter(paramNumber, paramValue);

        if (res)
        {
          request->send(200, F("text/json"), F("{\"SUCCESS\":true}"));
          return;
        }
        else
        {
          request->send(500, F("text/html"), F("Stove communication failed"));
          return;
        }
      }

      if (cmd.startsWith(F("SET HPAR ")))
      {
        bool res = true;

        String strHiddenParamNumber(cmd.substring(9, cmd.indexOf(' ', 9)));
        String strHiddenParamValue(cmd.substring(cmd.indexOf(' ', 9) + 1));

        byte hiddenParamNumber = strHiddenParamNumber.toInt();

        if (hiddenParamNumber == 0 && strHiddenParamNumber[0] != '0')
        {
          String ret(F("Incorrect Hidden Parameter Number : "));
          ret += strHiddenParamNumber;
          request->send(400, F("text/html"), ret);
          return;
        }

        uint16_t hiddenParamValue = strHiddenParamValue.toInt();

        if (hiddenParamValue == 0 && strHiddenParamValue[0] != '0')
        {
          String ret(F("Incorrect Hidden Parameter Value : "));
          ret += strHiddenParamValue;
          request->send(400, F("text/html"), ret);
          return;
        }

        res &= m_Pala.setHiddenParameter(hiddenParamNumber, hiddenParamValue);

        if (res)
        {
          request->send(200, F("text/json"), F("{\"SUCCESS\":true}"));
          return;
        }
        else
        {
          request->send(500, F("text/html"), F("Stove communication failed"));
          return;
        }
      }
    }

    //answer with error and return
    request->send(400, F("text/html"), F("No valid request received"));
  });
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
}