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
  String subscribeTopic = _ha.mqtt.generic.baseTopic;

  //Replace placeholders
  MQTTMan::prepareTopic(subscribeTopic);

  switch (_ha.mqtt.type) //switch on MQTT type
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

  bool isCmdExecuted = false;

  if (!isCmdExecuted && length == 6 && !memcmp_P(payload, F("CMD+ON"), length))
  {
    _Pala.powerOn();
    isCmdExecuted = true;
  }

  if (!isCmdExecuted && length == 7 && !memcmp_P(payload, F("CMD+OFF"), length))
  {
    _Pala.powerOff();
    isCmdExecuted = true;
  }

  if (!isCmdExecuted && length > 9 && length <= 14 && !memcmp_P(payload, F("SET+SETP+"), 9))
  {
    char strSetPoint[6];
    memcpy(strSetPoint, payload + 9, length - 9);
    strSetPoint[length - 9] = 0;

    float setPointFloat = atof(strSetPoint);

    if (setPointFloat != 0.0f)
    {
      _Pala.setSetpoint(setPointFloat);
      isCmdExecuted = true;
    }
  }

  if (!isCmdExecuted && length == 10 && !memcmp_P(payload, F("SET+POWR+"), 9))
  {
    if (payload[9] >= '1' && payload[9] <= '5')
    {
      _Pala.setPower(payload[9] - '0');
      isCmdExecuted = true;
    }
  }
  
  if (!isCmdExecuted && length == 10 && !memcmp_P(payload, F("SET+RFAN+"), 9))
  {
    if (payload[9] >= '0' && payload[9] <= '7')
    {
      _Pala.setRoomFan(payload[9] - '0');
      isCmdExecuted = true;
    }
  }

  //Finally if Cmd has been executed, Run a Publish to push back to HA
  if (isCmdExecuted)
    publishTick();
}

void WebPalaControl::publishTick()
{
  //if Home Automation upload not enabled then return
  if (_ha.protocol == HA_PROTO_DISABLED)
    return;

  //----- MQTT Protocol configured -----
  if (_ha.protocol == HA_PROTO_MQTT)
  {
    //if we are connected
    if (_mqttMan.connected())
    {
      //prepare base topic
      String baseTopic = _ha.mqtt.generic.baseTopic;

      //Replace placeholders
      MQTTMan::prepareTopic(baseTopic);

      _haSendResult = true;

      uint16_t STATUS, LSTATUS;
      if ((_haSendResult &= _Pala.getStatus(&STATUS, &LSTATUS)))
      {
        _haSendResult &= _mqttMan.publish((baseTopic + F("STATUS")).c_str(), String(STATUS).c_str());
        _statusEventSource.send((String("{\"STATUS\":") + STATUS + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("LSTATUS")).c_str(), String(LSTATUS).c_str());
        _statusEventSource.send((String("{\"LSTATUS\":") + LSTATUS + '}').c_str());
      }
      else
        return;

      float T1, T2, T3, T4, T5;
      if ((_haSendResult &= _Pala.getAllTemps(&T1, &T2, &T3, &T4, &T5)))
      {
        _haSendResult &= _mqttMan.publish((baseTopic + F("T1")).c_str(), String(T1).c_str());
        _statusEventSource.send((String("{\"T1\":") + T1 + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("T2")).c_str(), String(T2).c_str());
        _statusEventSource.send((String("{\"T2\":") + T2 + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("T3")).c_str(), String(T3).c_str());
        _statusEventSource.send((String("{\"T3\":") + T3 + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("T4")).c_str(), String(T4).c_str());
        _statusEventSource.send((String("{\"T4\":") + T4 + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("T5")).c_str(), String(T5).c_str());
        _statusEventSource.send((String("{\"T5\":") + T5 + '}').c_str());
      }
      else
        return;

      uint16_t F1V, F2V, F1RPM, F2L, F2LF, F3L, F4L;
      bool isF3LF4LValid;
      if ((_haSendResult &= _Pala.getFanData(&F1V, &F2V, &F1RPM, &F2L, &F2LF, &isF3LF4LValid, &F3L, &F4L)))
      {
        _haSendResult &= _mqttMan.publish((baseTopic + F("F1V")).c_str(), String(F1V).c_str());
        _statusEventSource.send((String("{\"F1V\":") + F1V + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("F2V")).c_str(), String(F2V).c_str());
        _statusEventSource.send((String("{\"F2V\":") + F2V + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("F2L")).c_str(), String(F2L).c_str());
        _statusEventSource.send((String("{\"F2L\":") + F2L + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("F2LF")).c_str(), String(F2LF).c_str());
        _statusEventSource.send((String("{\"F2LF\":") + F2LF + '}').c_str());
        if (isF3LF4LValid)
        {
          _haSendResult &= _mqttMan.publish((baseTopic + F("F3L")).c_str(), String(F3L).c_str());
          _statusEventSource.send((String("{\"F3L\":") + F3L + '}').c_str());
          _haSendResult &= _mqttMan.publish((baseTopic + F("F4L")).c_str(), String(F4L).c_str());
          _statusEventSource.send((String("{\"F4L\":") + F4L + '}').c_str());
        }
      }
      else
        return;

      uint16_t IGN, POWERTIMEh, POWERTIMEm, HEATTIMEh, HEATTIMEm, SERVICETIMEh, SERVICETIMEm, ONTIMEh, ONTIMEm, OVERTMPERRORS, IGNERRORS, PQTn;
      if ((_haSendResult &= _Pala.getCounters(&IGN, &POWERTIMEh, &POWERTIMEm, &HEATTIMEh, &HEATTIMEm, &SERVICETIMEh, &SERVICETIMEm, &ONTIMEh, &ONTIMEm, &OVERTMPERRORS, &IGNERRORS, &PQTn)))
      {
        _haSendResult &= _mqttMan.publish((baseTopic + F("IGN")).c_str(), String(IGN).c_str());
        _statusEventSource.send((String("{\"IGN\":") + IGN + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("IGNERRORS")).c_str(), String(IGNERRORS).c_str());
        _statusEventSource.send((String("{\"IGNERRORS\":") + IGNERRORS + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("POWERTIME")).c_str(), String(POWERTIMEh).c_str());
        _statusEventSource.send((String("{\"POWERTIME\":") + POWERTIMEh + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("HEATTIME")).c_str(), String(HEATTIMEh).c_str());
        _statusEventSource.send((String("{\"HEATTIME\":") + HEATTIMEh + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("SERVICETIME")).c_str(), String(SERVICETIMEh).c_str());
        _statusEventSource.send((String("{\"SERVICETIME\":") + SERVICETIMEh + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("ONTIME")).c_str(), String(ONTIMEh).c_str());
        _statusEventSource.send((String("{\"ONTIME\":") + ONTIMEh + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("OVERTMPERRORS")).c_str(), String(OVERTMPERRORS).c_str());
        _statusEventSource.send((String("{\"OVERTMPERRORS\":") + OVERTMPERRORS + '}').c_str());
      }
      else
        return;

      char STOVE_DATETIME[20];
      byte STOVE_WDAY;
      if ((_haSendResult &= _Pala.getDateTime(STOVE_DATETIME, &STOVE_WDAY)))
      {
        _haSendResult &= _mqttMan.publish((baseTopic + F("STOVE_DATETIME")).c_str(), String(STOVE_DATETIME).c_str());
        _statusEventSource.send((String("{\"STOVE_DATETIME\":") + STOVE_DATETIME + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("STOVE_WDAY")).c_str(), String(STOVE_WDAY).c_str());
        _statusEventSource.send((String("{\"STOVE_WDAY\":") + STOVE_WDAY + '}').c_str());
      }
      else
        return;

      float SETP;
      if ((_haSendResult &= _Pala.getSetPoint(&SETP)))
      {
        _haSendResult &= _mqttMan.publish((baseTopic + F("SETP")).c_str(), String(SETP).c_str());
        _statusEventSource.send((String("{\"SETP\":") + SETP + '}').c_str());
      }
      else
        return;

      uint16_t PQT;
      if ((_haSendResult &= _Pala.getPelletQtUsed(&PQT)))
      {
        _haSendResult &= _mqttMan.publish((baseTopic + F("PQT")).c_str(), String(PQT).c_str());
        _statusEventSource.send((String("{\"PQT\":") + PQT + '}').c_str());
      }
      else
        return;

      byte PWR;
      float FDR;
      if ((_haSendResult &= _Pala.getPower(&PWR, &FDR)))
      {
        _haSendResult &= _mqttMan.publish((baseTopic + F("PWR")).c_str(), String(PWR).c_str());
        _statusEventSource.send((String("{\"PWR\":") + PWR + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("FDR")).c_str(), String(FDR).c_str());
        _statusEventSource.send((String("{\"FDR\":") + FDR + '}').c_str());
      }
      else
        return;

      uint16_t DP_TARGET, DP_PRESS;
      if ((_haSendResult &= _Pala.getDPressData(&DP_TARGET, &DP_PRESS)))
      {
        _haSendResult &= _mqttMan.publish((baseTopic + F("DP_TARGET")).c_str(), String(DP_TARGET).c_str());
        _statusEventSource.send((String("{\"DP_TARGET\":") + DP_TARGET + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("DP_PRESS")).c_str(), String(DP_PRESS).c_str());
        _statusEventSource.send((String("{\"DP_PRESS\":") + DP_PRESS + '}').c_str());
      }
      else
        return;
    }
  }
}

//------------------------------------------
//Used to initialize configuration properties to default values
void WebPalaControl::setConfigDefaultValues()
{
  _ha.protocol = HA_PROTO_DISABLED;
  _ha.hostname[0] = 0;
  _ha.uploadPeriod = 60;

  _ha.mqtt.type = HA_MQTT_GENERIC;
  _ha.mqtt.port = 1883;
  _ha.mqtt.username[0] = 0;
  _ha.mqtt.password[0] = 0;
  _ha.mqtt.generic.baseTopic[0] = 0;
};
//------------------------------------------
//Parse JSON object into configuration properties
void WebPalaControl::parseConfigJSON(DynamicJsonDocument &doc)
{
  if (!doc[F("haproto")].isNull())
    _ha.protocol = doc[F("haproto")];
  if (!doc[F("hahost")].isNull())
    strlcpy(_ha.hostname, doc[F("hahost")], sizeof(_ha.hostname));
  if (!doc[F("haupperiod")].isNull())
    _ha.uploadPeriod = doc[F("haupperiod")];

  if (!doc[F("hamtype")].isNull())
    _ha.mqtt.type = doc[F("hamtype")];
  if (!doc[F("hamport")].isNull())
    _ha.mqtt.port = doc[F("hamport")];
  if (!doc[F("hamu")].isNull())
    strlcpy(_ha.mqtt.username, doc[F("hamu")], sizeof(_ha.mqtt.username));
  if (!doc[F("hamp")].isNull())
    strlcpy(_ha.mqtt.password, doc[F("hamp")], sizeof(_ha.mqtt.password));

  if (!doc[F("hamgbt")].isNull())
    strlcpy(_ha.mqtt.generic.baseTopic, doc[F("hamgbt")], sizeof(_ha.mqtt.generic.baseTopic));
};
//------------------------------------------
//Parse HTTP POST parameters in request into configuration properties
bool WebPalaControl::parseConfigWebRequest(AsyncWebServerRequest *request)
{

  //Parse HA protocol
  if (request->hasParam(F("haproto"), true))
    _ha.protocol = request->getParam(F("haproto"), true)->value().toInt();

  //if an home Automation protocol has been selected then get common param
  if (_ha.protocol != HA_PROTO_DISABLED)
  {
    if (request->hasParam(F("hahost"), true) && request->getParam(F("hahost"), true)->value().length() < sizeof(_ha.hostname))
      strcpy(_ha.hostname, request->getParam(F("hahost"), true)->value().c_str());
    if (request->hasParam(F("haupperiod"), true))
      _ha.uploadPeriod = request->getParam(F("haupperiod"), true)->value().toInt();
  }

  //Now get specific param
  switch (_ha.protocol)
  {

  case HA_PROTO_MQTT:

    if (request->hasParam(F("hamtype"), true))
      _ha.mqtt.type = request->getParam(F("hamtype"), true)->value().toInt();
    if (request->hasParam(F("hamport"), true))
      _ha.mqtt.port = request->getParam(F("hamport"), true)->value().toInt();
    if (request->hasParam(F("hamu"), true) && request->getParam(F("hamu"), true)->value().length() < sizeof(_ha.mqtt.username))
      strcpy(_ha.mqtt.username, request->getParam(F("hamu"), true)->value().c_str());
    char tempPassword[64 + 1] = {0};
    //put MQTT password into temporary one for predefpassword
    if (request->hasParam(F("hamp"), true) && request->getParam(F("hamp"), true)->value().length() < sizeof(tempPassword))
      strcpy(tempPassword, request->getParam(F("hamp"), true)->value().c_str());
    //check for previous password (there is a predefined special password that mean to keep already saved one)
    if (strcmp_P(tempPassword, appDataPredefPassword))
      strcpy(_ha.mqtt.password, tempPassword);

    switch (_ha.mqtt.type)
    {
    case HA_MQTT_GENERIC:
      if (request->hasParam(F("hamgbt"), true) && request->getParam(F("hamgbt"), true)->value().length() < sizeof(_ha.mqtt.generic.baseTopic))
        strcpy(_ha.mqtt.generic.baseTopic, request->getParam(F("hamgbt"), true)->value().c_str());

      if (!_ha.hostname[0] || !_ha.mqtt.generic.baseTopic[0])
        _ha.protocol = HA_PROTO_DISABLED;
      break;
    }
    break;
  }
  return true;
};
//------------------------------------------
//Generate JSON from configuration properties
String WebPalaControl::generateConfigJSON(bool forSaveFile = false)
{
  String gc('{');

  gc = gc + F("\"haproto\":") + _ha.protocol;
  gc = gc + F(",\"hahost\":\"") + _ha.hostname + '"';
  gc = gc + F(",\"haupperiod\":") + _ha.uploadPeriod;

  //if for WebPage or protocol selected is MQTT
  if (!forSaveFile || _ha.protocol == HA_PROTO_MQTT)
  {
    gc = gc + F(",\"hamtype\":") + _ha.mqtt.type;
    gc = gc + F(",\"hamport\":") + _ha.mqtt.port;
    gc = gc + F(",\"hamu\":\"") + _ha.mqtt.username + '"';
    if (forSaveFile)
      gc = gc + F(",\"hamp\":\"") + _ha.mqtt.password + '"';
    else
      gc = gc + F(",\"hamp\":\"") + (__FlashStringHelper *)appDataPredefPassword + '"'; //predefined special password (mean to keep already saved one)

    gc = gc + F(",\"hamgbt\":\"") + _ha.mqtt.generic.baseTopic + '"';
  }

  gc += '}';

  return gc;
};
//------------------------------------------
//Generate JSON of application status
String WebPalaControl::generateStatusJSON()
{
  String gs('{');

  gs = gs + F("\"has1\":\"");
  switch (_ha.protocol)
  {
  case HA_PROTO_DISABLED:
    gs = gs + F("Disabled");
    break;
  case HA_PROTO_MQTT:
    gs = gs + F("MQTT Connection State : ");
    switch (_mqttMan.state())
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

    if (_mqttMan.state() == MQTT_CONNECTED)
      gs = gs + F("\",\"has2\":\"Last Publish Result : ") + (_haSendResult ? F("OK") : F("Failed"));

    break;
  }
  gs += '"';

  gs += '}';

  return gs;
};
//------------------------------------------
//code to execute during initialization and reinitialization of the app
bool WebPalaControl::appInit(bool reInit)
{
  //Stop Publish
  _publishTicker.detach();

  //Stop MQTT
  _mqttMan.disconnect();

  //if MQTT used so configure it
  if (_ha.protocol == HA_PROTO_MQTT)
  {
    //prepare will topic
    String willTopic = _ha.mqtt.generic.baseTopic;
    MQTTMan::prepareTopic(willTopic);
    willTopic += F("connected");

    //setup MQTT
    _mqttMan.setClient(_wifiClient).setServer(_ha.hostname, _ha.mqtt.port);
    _mqttMan.setConnectedAndWillTopic(willTopic.c_str());
    _mqttMan.setConnectedCallback(std::bind(&WebPalaControl::mqttConnectedCallback, this, std::placeholders::_1, std::placeholders::_2));
    _mqttMan.setCallback(std::bind(&WebPalaControl::mqttCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    //Connect
    _mqttMan.connect(_ha.mqtt.username, _ha.mqtt.password);
  }

  bool res = true;
  res &= _Pala.initialize(
      std::bind(&WebPalaControl::myOpenSerial, this, std::placeholders::_1),
      std::bind(&WebPalaControl::myCloseSerial, this),
      std::bind(&WebPalaControl::mySelectSerial, this, std::placeholders::_1),
      std::bind(&WebPalaControl::myReadSerial, this, std::placeholders::_1, std::placeholders::_2),
      std::bind(&WebPalaControl::myWriteSerial, this, std::placeholders::_1, std::placeholders::_2),
      std::bind(&WebPalaControl::myDrainSerial, this),
      std::bind(&WebPalaControl::myFlushSerial, this),
      std::bind(&WebPalaControl::myUSleep, this, std::placeholders::_1));

  float setPoint;
  res &= _Pala.getSetPoint(&setPoint);
  if (res)
    LOG_SERIAL.printf("setpoint=%.2f", setPoint);

  //if no HA, then use default period for Convert
  if (_ha.protocol != HA_PROTO_DISABLED)
  {
    if (res)
      publishTick(); //if configuration changed, publish immediately
    _publishTicker.attach(_ha.uploadPeriod, [this]() { this->_needPublish = true; });
  }

  return res;
};
//------------------------------------------
//Return HTML Code to insert into Status Web page
const uint8_t *WebPalaControl::getHTMLContent(WebPageForPlaceHolder wp)
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
size_t WebPalaControl::getHTMLContentSize(WebPageForPlaceHolder wp)
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
void WebPalaControl::appInitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication)
{

  server.on("/cgi-bin/sendmsg.lua", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (request->hasParam(F("cmd")))
    {
      const String &cmd = request->getParam(F("cmd"))->value();

      if (cmd == F("GET ALLS"))
      {
        bool res = true;

        uint16_t STATUS;
        res &= _Pala.getStatus(&STATUS, nullptr);

        float T1;
        res &= _Pala.getAllTemps(&T1, nullptr, nullptr, nullptr, nullptr);

        float setPoint;
        res &= _Pala.getSetPoint(&setPoint);

        uint16_t PQT;
        res &= _Pala.getPelletQtUsed(&PQT);

        if (res)
        {
          DynamicJsonDocument doc(500);
          String jsonToReturn;
          doc[F("SUCCESS")] = true;
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
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"GET ALLS\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
          return;
        }
      }

      if (cmd == F("GET STAT"))
      {
        bool res = true;

        uint16_t STATUS, LSTATUS;
        res &= _Pala.getStatus(&STATUS, &LSTATUS);

        if (res)
        {
          DynamicJsonDocument doc(100);
          String jsonToReturn;
          doc[F("SUCCESS")] = true;
          JsonObject data = doc.createNestedObject(F("DATA"));
          data[F("STATUS")] = STATUS;
          data[F("LSTATUS")] = LSTATUS;
          serializeJson(doc, jsonToReturn);

          request->send(200, F("text/json"), jsonToReturn);
          return;
        }
        else
        {
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"GET STAT\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
          return;
        }
      }

      if (cmd == F("GET TMPS"))
      {
        bool res = true;

        float T1, T2, T3, T4, T5;
        res &= _Pala.getAllTemps(&T1, &T2, &T3, &T4, &T5);

        if (res)
        {
          DynamicJsonDocument doc(500);
          String jsonToReturn;
          doc[F("SUCCESS")] = true;
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
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"GET TMPS\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
          return;
        }
      }

      if (cmd == F("GET FAND"))
      {
        bool res = true;

        uint16_t F1V, F2V, F1RPM, F2L, F2LF, F3L, F4L;
        bool isF3LF4LValid;
        res &= _Pala.getFanData(&F1V, &F2V, &F1RPM, &F2L, &F2LF, &isF3LF4LValid, &F3L, &F4L);

        if (res)
        {
          DynamicJsonDocument doc(500);
          String jsonToReturn;
          doc[F("SUCCESS")] = true;
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
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"GET FAND\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
          return;
        }
      }

      if (cmd == F("GET SETP"))
      {
        bool res = true;

        float SETP;
        res &= _Pala.getSetPoint(&SETP);

        if (res)
        {
          DynamicJsonDocument doc(100);
          String jsonToReturn;
          doc[F("SUCCESS")] = true;
          JsonObject data = doc.createNestedObject(F("DATA"));
          data[F("SETP")] = SETP;
          serializeJson(doc, jsonToReturn);

          request->send(200, F("text/json"), jsonToReturn);
          return;
        }
        else
        {
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"GET SETP\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
          return;
        }
      }

      if (cmd == F("GET POWR"))
      {
        bool res = true;

        byte PWR;
        float FDR;
        res &= _Pala.getPower(&PWR, &FDR);

        if (res)
        {
          DynamicJsonDocument doc(100);
          String jsonToReturn;
          doc[F("SUCCESS")] = true;
          JsonObject data = doc.createNestedObject(F("DATA"));
          data[F("PWR")] = PWR;
          data[F("FDR")] = FDR;
          serializeJson(doc, jsonToReturn);

          request->send(200, F("text/json"), jsonToReturn);
          return;
        }
        else
        {
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"GET POWR\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
          return;
        }
      }

      if (cmd == F("GET CUNT") || cmd == F("GET CNTR"))
      {
        bool res = true;

        uint16_t IGN, POWERTIMEh, POWERTIMEm, HEATTIMEh, HEATTIMEm, SERVICETIMEh, SERVICETIMEm, ONTIMEh, ONTIMEm, OVERTMPERRORS, IGNERRORS, PQT;
        res &= _Pala.getCounters(&IGN, &POWERTIMEh, &POWERTIMEm, &HEATTIMEh, &HEATTIMEm, &SERVICETIMEh, &SERVICETIMEm, &ONTIMEh, &ONTIMEm, &OVERTMPERRORS, &IGNERRORS, &PQT);

        if (res)
        {
          DynamicJsonDocument doc(500);
          String jsonToReturn;
          doc[F("SUCCESS")] = true;
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
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"GET CNTR\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
          return;
        }
      }

      if (cmd == F("GET DPRS"))
      {
        bool res = true;

        uint16_t DP_TARGET, DP_PRESS;
        res &= _Pala.getDPressData(&DP_TARGET, &DP_PRESS);

        if (res)
        {
          DynamicJsonDocument doc(500);
          String jsonToReturn;
          doc[F("SUCCESS")] = true;
          JsonObject data = doc.createNestedObject(F("DATA"));
          data[F("DP_TARGET")] = DP_TARGET;
          data[F("DP_PRESS")] = DP_PRESS;
          serializeJson(doc, jsonToReturn);

          request->send(200, F("text/json"), jsonToReturn);
          return;
        }
        else
        {
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"GET DPRS\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
          return;
        }
      }

      if (cmd == F("GET TIME"))
      {
        bool res = true;

        char STOVE_DATETIME[20];
        byte STOVE_WDAY;
        res &= _Pala.getDateTime(STOVE_DATETIME, &STOVE_WDAY);

        if (res)
        {
          DynamicJsonDocument doc(100);
          String jsonToReturn;
          doc[F("SUCCESS")] = true;
          JsonObject data = doc.createNestedObject(F("DATA"));
          data[F("STOVE_DATETIME")] = STOVE_DATETIME;
          data[F("STOVE_WDAY")] = STOVE_WDAY;
          serializeJson(doc, jsonToReturn);

          request->send(200, F("text/json"), jsonToReturn);
          return;
        }
        else
        {
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"GET TIME\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
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
        res &= _Pala.getParameter(paramNumber, &paramValue);

        if (res)
        {
          DynamicJsonDocument doc(100);
          String jsonToReturn;
          doc[F("SUCCESS")] = true;
          JsonObject data = doc.createNestedObject(F("DATA"));
          data[F("PAR")] = paramValue;
          serializeJson(doc, jsonToReturn);

          request->send(200, F("text/json"), jsonToReturn);
          return;
        }
        else
        {
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"GET PARM\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
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
        res &= _Pala.getHiddenParameter(hiddenParamNumber, &hiddenParamValue);

        if (res)
        {
          DynamicJsonDocument doc(100);
          String jsonToReturn;
          doc[F("SUCCESS")] = true;
          JsonObject data = doc.createNestedObject(F("DATA"));
          data[F("HPAR")] = hiddenParamValue;
          serializeJson(doc, jsonToReturn);

          request->send(200, F("text/json"), jsonToReturn);
          return;
        }
        else
        {
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"GET HPAR\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
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
        res &= _Pala.getAllParameters(params);

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
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"BKP PARM\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
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
        res &= _Pala.getAllHiddenParameters(hiddenParams);

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
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"BKP HPAR\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
          return;
        }
      }

      if (cmd.startsWith(F("CMD ")))
      {
        bool res = true;

        if (cmd.substring(4) == F("ON"))
          res &= _Pala.powerOn();
        else if (cmd.substring(4) == F("OFF"))
          res &= _Pala.powerOff();
        else
          res = false;

        if (res)
        {
          request->send(200, F("text/json"), F("{\"SUCCESS\":true}"));
          return;
        }
        else
        {
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"CMD ON/OFF\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
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

        res &= _Pala.setPower(powerLevel);

        if (res)
        {
          request->send(200, F("text/json"), F("{\"SUCCESS\":true}"));
          return;
        }
        else
        {
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"SET POWR\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
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

        res &= _Pala.setRoomFan(roomFanLevel);

        if (res)
        {
          request->send(200, F("text/json"), F("{\"SUCCESS\":true}"));
          return;
        }
        else
        {
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"SET RFAN\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
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

        res &= _Pala.setRoomFan3(roomFan3Level);

        if (res)
        {
          request->send(200, F("text/json"), F("{\"SUCCESS\":true}"));
          return;
        }
        else
        {
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"SET FN3L\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
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

        res &= _Pala.setRoomFan4(roomFan4Level);

        if (res)
        {
          request->send(200, F("text/json"), F("{\"SUCCESS\":true}"));
          return;
        }
        else
        {
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"SET FN4L\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
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

        res &= _Pala.setSilentMode(silentMode);

        if (res)
        {
          request->send(200, F("text/json"), F("{\"SUCCESS\":true}"));
          return;
        }
        else
        {
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"SET SLNT\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
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

        res &= _Pala.setSetpoint(setPoint);

        if (res)
        {
          request->send(200, F("text/json"), F("{\"SUCCESS\":true}"));
          return;
        }
        else
        {
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"SET SETP\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
          return;
        }
      }

      if (cmd.startsWith(F("SET STPF ")))
      {
        bool res = true;

        float setPointFloat = cmd.substring(9).toFloat();

        if (setPointFloat == 0.0f)
        {
          String ret(F("Incorrect SetPoint Float value : "));
          ret += cmd;
          request->send(400, F("text/html"), ret);
          return;
        }

        res &= _Pala.setSetpoint(setPointFloat);

        if (res)
        {
          request->send(200, F("text/json"), F("{\"SUCCESS\":true}"));
          return;
        }
        else
        {
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"SET STPF\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
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

        res &= _Pala.setParameter(paramNumber, paramValue);

        if (res)
        {
          request->send(200, F("text/json"), F("{\"SUCCESS\":true}"));
          return;
        }
        else
        {
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"SET PARM\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
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

        res &= _Pala.setHiddenParameter(hiddenParamNumber, hiddenParamValue);

        if (res)
        {
          request->send(200, F("text/json"), F("{\"SUCCESS\":true}"));
          return;
        }
        else
        {
          request->send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"SET HPAR\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
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
void WebPalaControl::appRun()
{
  if (_ha.protocol == HA_PROTO_MQTT)
    _mqttMan.loop();

  if (_needPublish)
  {
    _needPublish = false;
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
