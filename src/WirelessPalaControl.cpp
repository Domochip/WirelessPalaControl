#include "WirelessPalaControl.h"

//Serial management functions -------------
int WebPalaControl::myOpenSerial(uint32_t baudrate)
{
  Serial.begin(baudrate);
  Serial.pins(15, 13); //swap ESP8266 pins to alternative positions (D7(GPIO13)(RX)/D8(GPIO15)(TX))
  return 0;
}
void WebPalaControl::myCloseSerial()
{
  Serial.end();
  //TX/GPIO15 is pulled down and so block the stove bus by default...
  pinMode(15, OUTPUT); //set TX PIN to OUTPUT HIGH
  digitalWrite(15, HIGH);
}
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

void WebPalaControl::mqttConnectedCallback(MQTTMan *mqttMan, bool firstConnection)
{

  //Subscribe to needed topic
  //prepare topic subscription
  String subscribeTopic = m_ha.mqtt.generic.baseTopic;

  //Replace placeholders
  MQTTMan::prepareTopic(subscribeTopic);

  switch (m_ha.mqtt.type) //switch on MQTT type
  {
  case HA_MQTT_GENERIC:
    subscribeTopic += F("cmd");
    break;
  }

  if (firstConnection)
    mqttMan->publish(subscribeTopic.c_str(), ""); //make empty publish only for firstConnection
  mqttMan->subscribe(subscribeTopic.c_str());
}

void WebPalaControl::mqttCallback(char *topic, uint8_t *payload, unsigned int length)
{
  //if topic is basetopic/cmd
  //commented because only this topic is subscribed

  if (length == 6 && !memcmp_P(payload, F("CMD+ON"), length))
  {
    m_Pala.powerOn();
    return;
  }

  if (length == 7 && !memcmp_P(payload, F("CMD+OFF"), length))
  {
    m_Pala.powerOff();
    return;
  }
}

void WebPalaControl::publishTick()
{
  //if Home Automation upload not enabled then return
  if (m_ha.protocol == HA_PROTO_DISABLED)
    return;

  //----- MQTT Protocol configured -----
  if (m_ha.protocol == HA_PROTO_MQTT)
  {
    //if we are connected
    if (m_mqttMan.connected())
    {
      //prepare base topic
      String baseTopic = m_ha.mqtt.generic.baseTopic;

      //Replace placeholders
      MQTTMan::prepareTopic(baseTopic);

      m_haSendResult = true;

      uint16_t STATUS, LSTATUS;
      if ((m_haSendResult &= m_Pala.getStatus(&STATUS, &LSTATUS)))
      {
        m_haSendResult &= m_mqttMan.publish((baseTopic + F("STATUS")).c_str(), String(STATUS).c_str());
        m_haSendResult &= m_mqttMan.publish((baseTopic + F("LSTATUS")).c_str(), String(LSTATUS).c_str());
      }
      else
        return;

      float T1, T2, T3, T4, T5;
      if ((m_haSendResult &= m_Pala.getAllTemps(&T1, &T2, &T3, &T4, &T5)))
      {
        m_haSendResult &= m_mqttMan.publish((baseTopic + F("T1")).c_str(), String(T1).c_str());
        m_haSendResult &= m_mqttMan.publish((baseTopic + F("T2")).c_str(), String(T2).c_str());
        m_haSendResult &= m_mqttMan.publish((baseTopic + F("T3")).c_str(), String(T3).c_str());
        m_haSendResult &= m_mqttMan.publish((baseTopic + F("T4")).c_str(), String(T4).c_str());
        m_haSendResult &= m_mqttMan.publish((baseTopic + F("T5")).c_str(), String(T5).c_str());
      }
      else
        return;

      uint16_t F1V, F2V, F1RPM, F2L, F2LF, F3L, F4L;
      bool isF3LF4LValid;
      if ((m_haSendResult &= m_Pala.getFanData(&F1V, &F2V, &F1RPM, &F2L, &F2LF, &isF3LF4LValid, &F3L, &F4L)))
      {
        m_haSendResult &= m_mqttMan.publish((baseTopic + F("F1V")).c_str(), String(F1V).c_str());
        m_haSendResult &= m_mqttMan.publish((baseTopic + F("F2V")).c_str(), String(F2V).c_str());
        m_haSendResult &= m_mqttMan.publish((baseTopic + F("F2L")).c_str(), String(F2L).c_str());
        m_haSendResult &= m_mqttMan.publish((baseTopic + F("F2LF")).c_str(), String(F2LF).c_str());
        if (isF3LF4LValid)
        {
          m_haSendResult &= m_mqttMan.publish((baseTopic + F("F3L")).c_str(), String(F3L).c_str());
          m_haSendResult &= m_mqttMan.publish((baseTopic + F("F4L")).c_str(), String(F4L).c_str());
        }
      }
      else
        return;

      float SETP;
      if ((m_haSendResult &= m_Pala.getSetPoint(&SETP)))
      {
        m_haSendResult &= m_mqttMan.publish((baseTopic + F("SETP")).c_str(), String(SETP).c_str());
      }
      else
        return;

      uint16_t PQT;
      if ((m_haSendResult &= m_Pala.getPelletQtUsed(&PQT)))
      {
        m_haSendResult &= m_mqttMan.publish((baseTopic + F("PQT")).c_str(), String(PQT).c_str());
      }
      else
        return;

      byte PWR;
      float FDR;
      if ((m_haSendResult &= m_Pala.getPower(&PWR, &FDR)))
      {
        m_haSendResult &= m_mqttMan.publish((baseTopic + F("PWR")).c_str(), String(PWR).c_str());
        m_haSendResult &= m_mqttMan.publish((baseTopic + F("FDR")).c_str(), String(FDR).c_str());
      }
      else
        return;

      uint16_t DP_TARGET, DP_PRESS;
      if ((m_haSendResult &= m_Pala.getDPressData(&DP_TARGET, &DP_PRESS)))
      {
        m_haSendResult &= m_mqttMan.publish((baseTopic + F("DP_TARGET")).c_str(), String(DP_TARGET).c_str());
        m_haSendResult &= m_mqttMan.publish((baseTopic + F("DP_PRESS")).c_str(), String(DP_PRESS).c_str());
      }
      else
        return;
    }
  }
}

//------------------------------------------
//Used to initialize configuration properties to default values
void WebPalaControl::SetConfigDefaultValues()
{
  m_ha.protocol = HA_PROTO_DISABLED;
  m_ha.hostname[0] = 0;
  m_ha.uploadPeriod = 60;

  m_ha.mqtt.type = HA_MQTT_GENERIC;
  m_ha.mqtt.port = 1883;
  m_ha.mqtt.username[0] = 0;
  m_ha.mqtt.password[0] = 0;
  m_ha.mqtt.generic.baseTopic[0] = 0;
};
//------------------------------------------
//Parse JSON object into configuration properties
void WebPalaControl::ParseConfigJSON(DynamicJsonDocument &doc)
{
  if (!doc[F("haproto")].isNull())
    m_ha.protocol = doc[F("haproto")];
  if (!doc[F("hahost")].isNull())
    strlcpy(m_ha.hostname, doc[F("hahost")], sizeof(m_ha.hostname));
  if (!doc[F("haupperiod")].isNull())
    m_ha.uploadPeriod = doc[F("haupperiod")];

  if (!doc[F("hamtype")].isNull())
    m_ha.mqtt.type = doc[F("hamtype")];
  if (!doc[F("hamport")].isNull())
    m_ha.mqtt.port = doc[F("hamport")];
  if (!doc[F("hamu")].isNull())
    strlcpy(m_ha.mqtt.username, doc[F("hamu")], sizeof(m_ha.mqtt.username));
  if (!doc[F("hamp")].isNull())
    strlcpy(m_ha.mqtt.password, doc[F("hamp")], sizeof(m_ha.mqtt.password));

  if (!doc[F("hamgbt")].isNull())
    strlcpy(m_ha.mqtt.generic.baseTopic, doc[F("hamgbt")], sizeof(m_ha.mqtt.generic.baseTopic));
};
//------------------------------------------
//Parse HTTP POST parameters in request into configuration properties
bool WebPalaControl::ParseConfigWebRequest(AsyncWebServerRequest *request)
{

  //Parse HA protocol
  if (request->hasParam(F("haproto"), true))
    m_ha.protocol = request->getParam(F("haproto"), true)->value().toInt();

  //if an home Automation protocol has been selected then get common param
  if (m_ha.protocol != HA_PROTO_DISABLED)
  {
    if (request->hasParam(F("hahost"), true) && request->getParam(F("hahost"), true)->value().length() < sizeof(m_ha.hostname))
      strcpy(m_ha.hostname, request->getParam(F("hahost"), true)->value().c_str());
    if (request->hasParam(F("haupperiod"), true))
      m_ha.uploadPeriod = request->getParam(F("haupperiod"), true)->value().toInt();
  }

  //Now get specific param
  switch (m_ha.protocol)
  {

  case HA_PROTO_MQTT:

    if (request->hasParam(F("hamtype"), true))
      m_ha.mqtt.type = request->getParam(F("hamtype"), true)->value().toInt();
    if (request->hasParam(F("hamport"), true))
      m_ha.mqtt.port = request->getParam(F("hamport"), true)->value().toInt();
    if (request->hasParam(F("hamu"), true) && request->getParam(F("hamu"), true)->value().length() < sizeof(m_ha.mqtt.username))
      strcpy(m_ha.mqtt.username, request->getParam(F("hamu"), true)->value().c_str());
    char tempPassword[64 + 1] = {0};
    //put MQTT password into temporary one for predefpassword
    if (request->hasParam(F("hamp"), true) && request->getParam(F("hamp"), true)->value().length() < sizeof(tempPassword))
      strcpy(tempPassword, request->getParam(F("hamp"), true)->value().c_str());
    //check for previous password (there is a predefined special password that mean to keep already saved one)
    if (strcmp_P(tempPassword, appDataPredefPassword))
      strcpy(m_ha.mqtt.password, tempPassword);

    switch (m_ha.mqtt.type)
    {
    case HA_MQTT_GENERIC:
      if (request->hasParam(F("hamgbt"), true) && request->getParam(F("hamgbt"), true)->value().length() < sizeof(m_ha.mqtt.generic.baseTopic))
        strcpy(m_ha.mqtt.generic.baseTopic, request->getParam(F("hamgbt"), true)->value().c_str());

      if (!m_ha.hostname[0] || !m_ha.mqtt.generic.baseTopic[0])
        m_ha.protocol = HA_PROTO_DISABLED;
      break;
    }
    break;
  }
  return true;
};
//------------------------------------------
//Generate JSON from configuration properties
String WebPalaControl::GenerateConfigJSON(bool forSaveFile = false)
{
  String gc('{');

  gc = gc + F("\"haproto\":") + m_ha.protocol;
  gc = gc + F(",\"hahost\":\"") + m_ha.hostname + '"';
  gc = gc + F(",\"haupperiod\":") + m_ha.uploadPeriod;

  //if for WebPage or protocol selected is MQTT
  if (!forSaveFile || m_ha.protocol == HA_PROTO_MQTT)
  {
    gc = gc + F(",\"hamtype\":") + m_ha.mqtt.type;
    gc = gc + F(",\"hamport\":") + m_ha.mqtt.port;
    gc = gc + F(",\"hamu\":\"") + m_ha.mqtt.username + '"';
    if (forSaveFile)
      gc = gc + F(",\"hamp\":\"") + m_ha.mqtt.password + '"';
    else
      gc = gc + F(",\"hamp\":\"") + (__FlashStringHelper *)appDataPredefPassword + '"'; //predefined special password (mean to keep already saved one)

    gc = gc + F(",\"hamgbt\":\"") + m_ha.mqtt.generic.baseTopic + '"';
  }

  gc += '}';

  return gc;
};
//------------------------------------------
//Generate JSON of application status
String WebPalaControl::GenerateStatusJSON()
{
  String gs('{');

  gs = gs + F("\"has1\":\"");
  switch (m_ha.protocol)
  {
  case HA_PROTO_DISABLED:
    gs = gs + F("Disabled");
    break;
  case HA_PROTO_MQTT:
    gs = gs + F("MQTT Connection State : ");
    switch (m_mqttMan.state())
    {
    case MQTT_CONNECTION_TIMEOUT:
      gs = gs + F("Timed Out");
      break;
    case MQTT_CONNECTION_LOST:
      gs = gs + F("Lost");
      break;
    case MQTT_CONNECT_FAILED:
      gs = gs + F("Failed");
      break;
    case MQTT_CONNECTED:
      gs = gs + F("Connected");
      break;
    case MQTT_CONNECT_BAD_PROTOCOL:
      gs = gs + F("Bad Protocol Version");
      break;
    case MQTT_CONNECT_BAD_CLIENT_ID:
      gs = gs + F("Incorrect ClientID ");
      break;
    case MQTT_CONNECT_UNAVAILABLE:
      gs = gs + F("Server Unavailable");
      break;
    case MQTT_CONNECT_BAD_CREDENTIALS:
      gs = gs + F("Bad Credentials");
      break;
    case MQTT_CONNECT_UNAUTHORIZED:
      gs = gs + F("Connection Unauthorized");
      break;
    }

    if (m_mqttMan.state() == MQTT_CONNECTED)
      gs = gs + F("\",\"has2\":\"Last Publish Result : ") + (m_haSendResult ? F("OK") : F("Failed"));

    break;
  }
  gs += '"';

  gs += '}';

  return gs;
};
//------------------------------------------
//code to execute during initialization and reinitialization of the app
bool WebPalaControl::AppInit(bool reInit)
{
  //Stop Publish
  m_publishTicker.detach();

  //Stop MQTT
  m_mqttMan.disconnect();

  //if MQTT used so configure it
  if (m_ha.protocol == HA_PROTO_MQTT)
  {
    //prepare will topic
    String willTopic = m_ha.mqtt.generic.baseTopic;
    MQTTMan::prepareTopic(willTopic);
    willTopic += F("connected");

    //setup MQTT
    m_mqttMan.setClient(m_wifiClient).setServer(m_ha.hostname, m_ha.mqtt.port);
    m_mqttMan.setConnectedAndWillTopic(willTopic.c_str());
    m_mqttMan.setConnectedCallback(std::bind(&WebPalaControl::mqttConnectedCallback, this, std::placeholders::_1, std::placeholders::_2));
    m_mqttMan.setCallback(std::bind(&WebPalaControl::mqttCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    //Connect
    m_mqttMan.connect(m_ha.mqtt.username, m_ha.mqtt.password);
  }

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

  //if no HA, then use default period for Convert
  if (m_ha.protocol != HA_PROTO_DISABLED)
  {
    if (res)
      publishTick(); //if configuration changed, publish immediately
    m_publishTicker.attach(m_ha.uploadPeriod, [this]() { this->m_needPublish = true; });
  }

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

      if (cmd == F("GET STAT"))
      {
        bool res = true;

        uint16_t STATUS, LSTATUS;
        res &= m_Pala.getStatus(&STATUS, &LSTATUS);

        if (res)
        {
          DynamicJsonDocument doc(100);
          String jsonToReturn;
          JsonObject data = doc.createNestedObject(F("DATA"));
          data[F("STATUS")] = STATUS;
          data[F("LSTATUS")] = LSTATUS;
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

      if (cmd == F("GET CUNT") || cmd == F("GET CNTR"))
      {
        bool res = true;

        uint16_t IGN, POWERTIMEh, POWERTIMEm, HEATTIMEh, HEATTIMEm, SERVICETIMEh, SERVICETIMEm, ONTIMEh, ONTIMEm, OVERTMPERRORS, IGNERRORS, PQT;
        res &= m_Pala.getCounters(&IGN, &POWERTIMEh, &POWERTIMEm, &HEATTIMEh, &HEATTIMEm, &SERVICETIMEh, &SERVICETIMEm, &ONTIMEh, &ONTIMEm, &OVERTMPERRORS, &IGNERRORS, &PQT);

        if (res)
        {
          DynamicJsonDocument doc(500);
          String jsonToReturn;
          JsonObject data = doc.createNestedObject(F("DATA"));
          data[F("IGN")] = IGN;
          data[F("POWERTIME")] = String(POWERTIMEh) + ':' + (POWERTIMEm / 10) + (POWERTIMEm % 10);
          data[F("HEATTIME")] = String(HEATTIMEh) + ':' + (HEATTIMEm / 10) + (HEATTIMEm % 10);
          data[F("SERVICETIME")] = String(SERVICETIMEh) + ':' + (SERVICETIMEm / 10) + (SERVICETIMEm % 10);
          data[F("ONTIME")] = String(ONTIMEh) + ':' + (ONTIMEm / 10) + (ONTIMEm % 10);
          data[F("OVERTMPERRORS")] = OVERTMPERRORS;
          data[F("IGNERRORS")] = IGNERRORS;
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

      if (cmd == F("GET DPRS"))
      {
        bool res = true;

        uint16_t DP_TARGET, DP_PRESS;
        res &= m_Pala.getDPressData(&DP_TARGET, &DP_PRESS);

        if (res)
        {
          DynamicJsonDocument doc(500);
          String jsonToReturn;
          JsonObject data = doc.createNestedObject(F("DATA"));
          data[F("DP_TARGET")] = DP_TARGET;
          data[F("DP_PRESS")] = DP_PRESS;
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

      if (cmd == F("GET TIME"))
      {
        bool res = true;

        char STOVE_DATETIME[20];
        byte STOVE_WDAY;
        res &= m_Pala.getDateTime(STOVE_DATETIME, &STOVE_WDAY);

        if (res)
        {
          DynamicJsonDocument doc(100);
          String jsonToReturn;
          JsonObject data = doc.createNestedObject(F("DATA"));
          data[F("STOVE_DATETIME")] = STOVE_DATETIME;
          data[F("STOVE_WDAY")] = STOVE_WDAY;
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

      if (cmd.startsWith(F("BKP PARM ")))
      {
        bool res = true;

        byte fileType;

        String strFileType(cmd.substring(9));

        if (strFileType == F("CSV"))
          fileType = 0;
        else if (strFileType == F("JSON"))
          fileType = 1;
        else
        {
          String ret(F("Incorrect File Type : "));
          ret += cmd.substring(9);
          request->send(400, F("text/html"), ret);
          return;
        }

        byte params[0x6A];
        res &= m_Pala.getAllParameters(params);

        if (res)
        {
          String toReturn;
          AsyncWebServerResponse *response;

          switch (fileType)
          {
          case 0: //CSV
            toReturn += F("PARM;VALUE\r\n");
            for (byte i = 0; i < 0x6A; i++)
              toReturn += String(i) + ';' + params[i] + '\r' + '\n';

            response = request->beginResponse(200, F("text/csv"), toReturn);
            response->addHeader("Content-Disposition", "attachment; filename=\"PARM.csv\"");
            request->send(response);

            break;
          case 1: //JSON
            toReturn += F("{\"PARM\":[");
            for (byte i = 0; i < 0x6A; i++)
            {
              if (i)
                toReturn += F(",\r\n");
              toReturn += params[i];
            }
            toReturn += F("]}");

            response = request->beginResponse(200, F("text/json"), toReturn);
            response->addHeader("Content-Disposition", "attachment; filename=\"PARM.json\"");
            request->send(response);

            break;
          }

          return;
        }
        else
        {
          request->send(500, F("text/html"), F("Stove communication failed"));
          return;
        }
      }

      if (cmd.startsWith(F("BKP HPAR ")))
      {
        bool res = true;

        byte fileType;

        String strFileType(cmd.substring(9));

        if (strFileType == F("CSV"))
          fileType = 0;
        else if (strFileType == F("JSON"))
          fileType = 1;
        else
        {
          String ret(F("Incorrect File Type : "));
          ret += cmd.substring(9);
          request->send(400, F("text/html"), ret);
          return;
        }

        uint16_t hiddenParams[0x6F];
        res &= m_Pala.getAllHiddenParameters(hiddenParams);

        if (res)
        {
          String toReturn;
          AsyncWebServerResponse *response;

          switch (fileType)
          {
          case 0: //CSV
            toReturn += F("HPAR;VALUE\r\n");
            for (byte i = 0; i < 0x6F; i++)
              toReturn += String(i) + ';' + hiddenParams[i] + '\r' + '\n';

            response = request->beginResponse(200, F("text/csv"), toReturn);
            response->addHeader("Content-Disposition", "attachment; filename=\"HPAR.csv\"");
            request->send(response);

            break;
          case 1: //JSON
            toReturn += F("{\"HPAR\":[");
            for (byte i = 0; i < 0x6F; i++)
            {
              if (i)
                toReturn += F(",\r\n");
              toReturn += hiddenParams[i];
            }
            toReturn += F("]}");

            response = request->beginResponse(200, F("text/json"), toReturn);
            response->addHeader("Content-Disposition", "attachment; filename=\"HPAR.json\"");
            request->send(response);

            break;
          }

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

      if (cmd.startsWith(F("SET FN3L ")))
      {
        bool res = true;

        String strRoomFan3Level(cmd.substring(9));

        byte roomFan3Level = strRoomFan3Level.toInt();

        if (roomFan3Level == 0 && strRoomFan3Level[0] != '0')
        {
          String ret(F("Incorrect Room Fan 3 value : "));
          ret += cmd;
          request->send(400, F("text/html"), ret);
          return;
        }

        res &= m_Pala.setRoomFan3(roomFan3Level);

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

      if (cmd.startsWith(F("SET FN4L ")))
      {
        bool res = true;

        String strRoomFan4Level(cmd.substring(9));

        byte roomFan4Level = strRoomFan4Level.toInt();

        if (roomFan4Level == 0 && strRoomFan4Level[0] != '0')
        {
          String ret(F("Incorrect Room Fan 4 value : "));
          ret += cmd;
          request->send(400, F("text/html"), ret);
          return;
        }

        res &= m_Pala.setRoomFan4(roomFan4Level);

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

      if (cmd.startsWith(F("SET SLNT ")))
      {
        bool res = true;

        byte silentMode = cmd.substring(9).toInt();

        if (silentMode == 0)
        {
          String ret(F("Incorrect Silent Mode value : "));
          ret += cmd;
          request->send(400, F("text/html"), ret);
          return;
        }

        res &= m_Pala.setSilentMode(silentMode);

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
  if (m_ha.protocol == HA_PROTO_MQTT)
    m_mqttMan.loop();

  if (m_needPublish)
  {
    m_needPublish = false;
    LOG_SERIAL.println(F("PublishTick"));
    publishTick();
  }
}

//------------------------------------------
//Constructor
WebPalaControl::WebPalaControl(char appId, String appName) : Application(appId, appName)
{
  //TX/GPIO15 is pulled down and so block the stove bus by default...
  pinMode(15, OUTPUT); //set TX PIN to OUTPUT HIGH to unlock bus during WiFi connection
  digitalWrite(15, HIGH);
}