#include "WirelessPalaControl.h"

// Serial management functions -------------
int WebPalaControl::myOpenSerial(uint32_t baudrate)
{
  Serial.begin(baudrate);
  Serial.pins(15, 13); // swap ESP8266 pins to alternative positions (D7(GPIO13)(RX)/D8(GPIO15)(TX))
  return 0;
}
void WebPalaControl::myCloseSerial()
{
  Serial.end();
  // TX/GPIO15 is pulled down and so block the stove bus by default...
  pinMode(15, OUTPUT); // set TX PIN to OUTPUT HIGH
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
    ESP.wdtFeed(); // feed the WDT to prevent bite
  }

  if (Serial.available())
    return 1;
  return 0;
}
size_t WebPalaControl::myReadSerial(void *buf, size_t count) { return Serial.read((char *)buf, count); }
size_t WebPalaControl::myWriteSerial(const void *buf, size_t count) { return Serial.write((const uint8_t *)buf, count); }
int WebPalaControl::myDrainSerial()
{
  Serial.flush(); // On ESP, Serial.flush() is drain
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

  // Subscribe to needed topic
  // prepare topic subscription
  String subscribeTopic = _ha.mqtt.generic.baseTopic;

  // Replace placeholders
  MQTTMan::prepareTopic(subscribeTopic);

  switch (_ha.mqtt.type) // switch on MQTT type
  {
  case HA_MQTT_GENERIC:
  case HA_MQTT_GENERIC_JSON:
  case HA_MQTT_GENERIC_CATEGORIZED:
    subscribeTopic += F("cmd");
    break;
  }

  if (firstConnection)
    mqttMan->publish(subscribeTopic.c_str(), ""); // make empty publish only for firstConnection
  mqttMan->subscribe(subscribeTopic.c_str());
}

void WebPalaControl::mqttCallback(char *topic, uint8_t *payload, unsigned int length)
{
  // if topic is basetopic/cmd
  // commented because only this topic is subscribed

  String cmd;
  DynamicJsonDocument jsonDoc(2048);

  // convert payload to String cmd
  cmd.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++)
    cmd += (char)payload[i];

  // replace '+' by ' '
  cmd.replace('+', ' ');

  // execute Palazzetti command
  bool isCmdExecuted = executePalaCmd(cmd, jsonDoc);

  // publish json result to MQTT
  String baseTopic = _ha.mqtt.generic.baseTopic;
  MQTTMan::prepareTopic(baseTopic);
  String serializedData;
  serializeJson(jsonDoc, serializedData);
  _mqttMan.publish((baseTopic + F("result")).c_str(), serializedData.c_str());
  _mqttMan.loop();

  // if Palazzetti command execution has been successful, Run a publish to push back to HA
  if (isCmdExecuted)
    publishTick();
}

bool WebPalaControl::publishDataToMqtt(const String &baseTopic, const String &palaCategory, const DynamicJsonDocument &jsonDoc)
{
  bool res = false;
  if (_mqttMan.connected())
  {
    String serializedData;
    String topic;

    switch (_ha.mqtt.type) // switch on MQTT type
    {
    case HA_MQTT_GENERIC:
      // for each key/value pair in DATA
      for (JsonPairConst kv : jsonDoc[F("DATA")].as<JsonObjectConst>())
      {
        // prepare topic
        topic = baseTopic;
        topic += kv.key().c_str();
        // prepare value
        String value = kv.value().as<String>();
        // publish
        res = _mqttMan.publish(topic.c_str(), value.c_str());
        _mqttMan.loop();
      }
      break;
    case HA_MQTT_GENERIC_JSON:
      // prepare topic
      topic = baseTopic;
      topic += palaCategory;
      // serialize DATA to JSON
      serializeJson(jsonDoc[F("DATA")], serializedData);
      // publish
      res = _mqttMan.publish(topic.c_str(), serializedData.c_str());
      _mqttMan.loop();
      break;
    case HA_MQTT_GENERIC_CATEGORIZED:
      // prepare category topic
      String categoryTopic(baseTopic);
      categoryTopic += palaCategory;
      categoryTopic += '/';

      // for each key/value pair in DATA
      for (JsonPairConst kv : jsonDoc[F("DATA")].as<JsonObjectConst>())
      {
        // prepare topic
        topic = categoryTopic;
        topic += kv.key().c_str();
        // prepare value
        String value = kv.value().as<String>();
        // publish
        res = _mqttMan.publish(topic.c_str(), value.c_str());
        _mqttMan.loop();
      }
      break;
    }
  }
  return res;
}

void WebPalaControl::publishTick()
{
  // Execute a defined list of commands and publish results to MQTT if needed

  bool execSuccess = true;
  _haSendResult = true;
  DynamicJsonDocument jsonDoc(2048);
  String baseTopic = _ha.mqtt.generic.baseTopic;

  MQTTMan::prepareTopic(baseTopic);

  // create an array of commands to execute
  String cmdList[] = {
      F("STAT"),
      F("TMPS"),
      F("FAND"),
      F("CNTR"),
      F("TIME"),
      F("SETP"),
      F("POWR"),
      F("DPRS")};

  // execute commands
  for (String cmd : cmdList)
  {
    String getCmd(F("GET "));
    getCmd += cmd;
    if (execSuccess &= executePalaCmd(getCmd, jsonDoc))
    {
      if (_ha.protocol == HA_PROTO_MQTT && _haSendResult)
      {
        _haSendResult &= publishDataToMqtt(baseTopic, cmd, jsonDoc);
      }
    }
    jsonDoc.clear();
  }
}

bool WebPalaControl::executePalaCmd(const String &cmd, DynamicJsonDocument &jsonDoc)
{
  bool cmdProcessed = false; // cmd has been processed
  bool cmdSuccess = false;   // Palazzetti function calls successful

  // Prepare answer structure
  JsonObject info = jsonDoc.createNestedObject(F("INFO"));
  JsonObject data = jsonDoc.createNestedObject(F("DATA"));

  if (!cmdProcessed && cmd == F("GET STDT"))
  {
    cmdProcessed = true;

    char SN[28];
    byte SNCHK;
    int MBTYPE;
    uint16_t MOD, VER, CORE;
    char FWDATE[11];
    uint16_t FLUID;
    uint16_t SPLMIN, SPLMAX;
    byte UICONFIG;
    byte HWTYPE;
    uint16_t DSPFWVER;
    byte CONFIG;
    byte PELLETTYPE;
    uint16_t PSENSTYPE;
    byte PSENSLMAX, PSENSLTSH, PSENSLMIN;
    byte MAINTPROBE;
    byte STOVETYPE;
    byte FAN2TYPE;
    byte FAN2MODE;
    byte BLEMBMODE;
    byte BLEDSPMODE;
    byte CHRONOTYPE;
    byte AUTONOMYTYPE;
    byte NOMINALPWR;
    cmdSuccess = _Pala.getStaticData(&SN, &SNCHK, &MBTYPE, &MOD, &VER, &CORE, &FWDATE, &FLUID, &SPLMIN, &SPLMAX, &UICONFIG, &HWTYPE, &DSPFWVER, &CONFIG, &PELLETTYPE, &PSENSTYPE, &PSENSLMAX, &PSENSLTSH, &PSENSLMIN, &MAINTPROBE, &STOVETYPE, &FAN2TYPE, &FAN2MODE, &BLEMBMODE, &BLEDSPMODE, &CHRONOTYPE, &AUTONOMYTYPE, &NOMINALPWR);

    if (cmdSuccess)
    {
      // ----- WPalaControl generated values -----
      data[F("LABEL")] = WiFi.getHostname();

      // Network infos
      data[F("GWDEVICE")] = F("wlan0"); // always wifi
      data[F("MAC")] = WiFi.macAddress();
      data[F("GATEWAY")] = WiFi.gatewayIP().toString();
      JsonArray dns = data.createNestedArray("DNS");
      dns.add(WiFi.dnsIP().toString());

      // Wifi infos
      data[F("WMAC")] = WiFi.macAddress();
      data[F("WMODE")] = (WiFi.getMode() & WIFI_STA) ? F("sta") : F("ap");
      data[F("WADR")] = (WiFi.getMode() & WIFI_STA) ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
      data[F("WGW")] = WiFi.gatewayIP().toString();
      data[F("WENC")] = F("psk2");
      data[F("WPWR")] = String(WiFi.RSSI()) + F(" dBm"); // need conversion to dBm?
      data[F("WSSID")] = WiFi.SSID();
      data[F("WPR")] = (true) ? F("dhcp") : F("static");
      data[F("WMSK")] = WiFi.subnetMask().toString();
      data[F("WBCST")] = WiFi.broadcastIP().toString();
      data[F("WCH")] = String(WiFi.channel());

      // Ethernet infos
      data[F("EPR")] = F("dhcp");
      data[F("EGW")] = F("0.0.0.0");
      data[F("EMSK")] = F("0.0.0.0");
      data[F("EADR")] = F("0.0.0.0");
      data[F("EMAC")] = WiFi.macAddress();
      data[F("ECBL")] = F("down");
      data[F("EBCST")] = "";

      data[F("APLCONN")] = 1; // appliance connected
      data[F("ICONN")] = 0;   // internet connected

      data[F("CBTYPE")] = F("miniembplug"); // CBox model
      data[F("sendmsg")] = F("2.1.2 2018-03-28 10:19:09");
      data[F("plzbridge")] = F("2.2.1 2021-10-08 09:30:45");
      data[F("SYSTEM")] = F("2.5.3 2021-10-08 10:30:20 (657c8cf)");

      data[F("CLOUD_ENABLED")] = true;

      // ----- Values from stove -----
      data[F("SN")] = SN;
      data[F("SNCHK")] = SNCHK;
      data[F("MBTYPE")] = MBTYPE;
      data[F("MOD")] = MOD;
      data[F("VER")] = VER;
      data[F("CORE")] = CORE;
      data[F("FWDATE")] = FWDATE;
      data[F("FLUID")] = FLUID;
      data[F("SPLMIN")] = SPLMIN;
      data[F("SPLMAX")] = SPLMAX;
      data[F("UICONFIG")] = UICONFIG;
      data[F("HWTYPE")] = HWTYPE;
      data[F("DSPFWVER")] = DSPFWVER;
      data[F("CONFIG")] = CONFIG;
      data[F("PELLETTYPE")] = PELLETTYPE;
      data[F("PSENSTYPE")] = PSENSTYPE;
      data[F("PSENSLMAX")] = PSENSLMAX;
      data[F("PSENSLTSH")] = PSENSLTSH;
      data[F("PSENSLMIN")] = PSENSLMIN;
      data[F("MAINTPROBE")] = MAINTPROBE;
      data[F("STOVETYPE")] = STOVETYPE;
      data[F("FAN2TYPE")] = FAN2TYPE;
      data[F("FAN2MODE")] = FAN2MODE;
      data[F("BLEMBMODE")] = BLEMBMODE;
      data[F("BLEDSPMODE")] = BLEDSPMODE;
      data[F("CHRONOTYPE")] = 0; // disable chronothermostat (no planning) (enabled if > 1)
      data[F("AUTONOMYTYPE")] = AUTONOMYTYPE;
      data[F("NOMINALPWR")] = NOMINALPWR;
    }
  }

  if (!cmdProcessed && cmd == F("GET LABL"))
  {
    cmdProcessed = true;
    cmdSuccess = true;

    data[F("LABEL")] = WiFi.getHostname();
  }

  if (!cmdProcessed && cmd == F("GET ALLS"))
  {
    cmdProcessed = true;

    bool refreshStatus = false;
    unsigned long currentMillis = millis();
    if ((currentMillis - _lastAllStatusRefreshMillis) > 15000UL) // refresh AllStatus data if it's 15sec old
      refreshStatus = true;

    int MBTYPE;
    uint16_t MOD, VER, CORE;
    char FWDATE[11];
    char APLTS[20];
    uint16_t APLWDAY;
    byte CHRSTATUS;
    uint16_t STATUS, LSTATUS;
    bool isMFSTATUSValid;
    uint16_t MFSTATUS;
    float SETP;
    byte PUMP;
    uint16_t PQT;
    uint16_t F1V;
    uint16_t F1RPM;
    uint16_t F2L;
    uint16_t F2LF;
    uint16_t FANLMINMAX[6];
    uint16_t F2V;
    bool isF3LF4LValid;
    uint16_t F3L;
    uint16_t F4L;
    byte PWR;
    float FDR;
    uint16_t DPT;
    uint16_t DP;
    byte IN;
    byte OUT;
    float T1, T2, T3, T4, T5;
    bool isSNValid;
    char SN[28];
    cmdSuccess = _Pala.getAllStatus(refreshStatus, &MBTYPE, &MOD, &VER, &CORE, &FWDATE, &APLTS, &APLWDAY, &CHRSTATUS, &STATUS, &LSTATUS, &isMFSTATUSValid, &MFSTATUS, &SETP, &PUMP, &PQT, &F1V, &F1RPM, &F2L, &F2LF, &FANLMINMAX, &F2V, &isF3LF4LValid, &F3L, &F4L, &PWR, &FDR, &DPT, &DP, &IN, &OUT, &T1, &T2, &T3, &T4, &T5, &isSNValid, &SN);

    if (cmdSuccess)
    {
      if (refreshStatus)
        _lastAllStatusRefreshMillis = currentMillis;

      data[F("MBTYPE")] = MBTYPE;
      data[F("MAC")] = WiFi.macAddress();
      data[F("MOD")] = MOD;
      data[F("VER")] = VER;
      data[F("CORE")] = CORE;
      data[F("FWDATE")] = FWDATE;
      data[F("APLTS")] = APLTS;
      data[F("APLWDAY")] = APLWDAY;
      data[F("CHRSTATUS")] = CHRSTATUS;
      data[F("STATUS")] = STATUS;
      data[F("LSTATUS")] = LSTATUS;
      if (isMFSTATUSValid)
        data[F("MFSTATUS")] = MFSTATUS;
      data[F("SETP")] = serialized(String(SETP, 2));
      data[F("PUMP")] = PUMP;
      data[F("PQT")] = PQT;
      data[F("F1V")] = F1V;
      data[F("F1RPM")] = F1RPM;
      data[F("F2L")] = F2L;
      data[F("F2LF")] = F2LF;
      JsonArray fanlminmax = data.createNestedArray(F("FANLMINMAX"));
      fanlminmax.add(FANLMINMAX[0]);
      fanlminmax.add(FANLMINMAX[1]);
      fanlminmax.add(FANLMINMAX[2]);
      fanlminmax.add(FANLMINMAX[3]);
      fanlminmax.add(FANLMINMAX[4]);
      fanlminmax.add(FANLMINMAX[5]);
      data[F("F2V")] = F2V;
      if (isF3LF4LValid)
      {
        data[F("F3L")] = F3L;
        data[F("F4L")] = F4L;
      }
      data[F("PWR")] = PWR;
      data[F("FDR")] = serialized(String(FDR, 2));
      data[F("DPT")] = DPT;
      data[F("DP")] = DP;
      data[F("IN")] = IN;
      data[F("OUT")] = OUT;
      data[F("T1")] = serialized(String(T1, 2));
      data[F("T2")] = serialized(String(T2, 2));
      data[F("T3")] = serialized(String(T3, 2));
      data[F("T4")] = serialized(String(T4, 2));
      data[F("T5")] = serialized(String(T5, 2));

      data[F("EFLAGS")] = 0; // new ErrorFlags not implemented
      if (isSNValid)
        data[F("SN")] = SN;
    }
  }

  if (!cmdProcessed && cmd == F("GET STAT"))
  {
    cmdProcessed = true;

    uint16_t STATUS, LSTATUS;
    cmdSuccess = _Pala.getStatus(&STATUS, &LSTATUS);

    if (cmdSuccess)
    {
      data[F("STATUS")] = STATUS;
      data[F("LSTATUS")] = LSTATUS;
    }
  }

  if (!cmdProcessed && cmd == F("GET TMPS"))
  {
    cmdProcessed = true;

    float T1, T2, T3, T4, T5;
    cmdSuccess = _Pala.getAllTemps(&T1, &T2, &T3, &T4, &T5);

    if (cmdSuccess)
    {
      data[F("T1")] = serialized(String(T1, 2));
      data[F("T2")] = serialized(String(T2, 2));
      data[F("T3")] = serialized(String(T3, 2));
      data[F("T4")] = serialized(String(T4, 2));
      data[F("T5")] = serialized(String(T5, 2));
    }
  }

  if (!cmdProcessed && cmd == F("GET FAND"))
  {
    cmdProcessed = true;

    uint16_t F1V, F2V, F1RPM, F2L, F2LF;
    bool isF3SF4SValid;
    float F3S, F4S;
    bool isF3LF4LValid;
    uint16_t F3L, F4L;
    cmdSuccess = _Pala.getFanData(&F1V, &F2V, &F1RPM, &F2L, &F2LF, &isF3SF4SValid, &F3S, &F4S, &isF3LF4LValid, &F3L, &F4L);

    if (cmdSuccess)
    {
      data[F("F1V")] = F1V;
      data[F("F2V")] = F2V;
      data[F("F1RPM")] = F1RPM;
      data[F("F2L")] = F2L;
      data[F("F2LF")] = F2LF;
      if (isF3SF4SValid)
      {
        data[F("F3S")] = serialized(String(F3S, 2));
        data[F("F4S")] = serialized(String(F4S, 2));
      }
      if (isF3LF4LValid)
      {
        data[F("F3L")] = F3L;
        data[F("F4L")] = F4L;
      }
    }
  }

  if (!cmdProcessed && cmd == F("GET SETP"))
  {
    cmdProcessed = true;

    float SETP;
    cmdSuccess = _Pala.getSetPoint(&SETP);

    if (cmdSuccess)
    {
      data[F("SETP")] = serialized(String(SETP, 2));
    }
  }

  if (!cmdProcessed && cmd == F("GET POWR"))
  {
    cmdProcessed = true;

    byte PWR;
    float FDR;
    cmdSuccess = _Pala.getPower(&PWR, &FDR);

    if (cmdSuccess)
    {
      data[F("PWR")] = PWR;
      data[F("FDR")] = serialized(String(FDR, 2));
    }
  }

  if (!cmdProcessed && (cmd == F("GET CUNT") || cmd == F("GET CNTR")))
  {
    cmdProcessed = true;

    uint16_t IGN, POWERTIMEh, POWERTIMEm, HEATTIMEh, HEATTIMEm, SERVICETIMEh, SERVICETIMEm, ONTIMEh, ONTIMEm, OVERTMPERRORS, IGNERRORS, PQT;
    cmdSuccess = _Pala.getCounters(&IGN, &POWERTIMEh, &POWERTIMEm, &HEATTIMEh, &HEATTIMEm, &SERVICETIMEh, &SERVICETIMEm, &ONTIMEh, &ONTIMEm, &OVERTMPERRORS, &IGNERRORS, &PQT);

    if (cmdSuccess)
    {
      data[F("IGN")] = IGN;
      data[F("POWERTIME")] = String(POWERTIMEh) + ':' + (POWERTIMEm / 10) + (POWERTIMEm % 10);
      data[F("HEATTIME")] = String(HEATTIMEh) + ':' + (HEATTIMEm / 10) + (HEATTIMEm % 10);
      data[F("SERVICETIME")] = String(SERVICETIMEh) + ':' + (SERVICETIMEm / 10) + (SERVICETIMEm % 10);
      data[F("ONTIME")] = String(ONTIMEh) + ':' + (ONTIMEm / 10) + (ONTIMEm % 10);
      data[F("OVERTMPERRORS")] = OVERTMPERRORS;
      data[F("IGNERRORS")] = IGNERRORS;
      data[F("PQT")] = PQT;
    }
  }

  if (!cmdProcessed && cmd == F("GET DPRS"))
  {
    cmdProcessed = true;

    uint16_t DP_TARGET, DP_PRESS;
    cmdSuccess = _Pala.getDPressData(&DP_TARGET, &DP_PRESS);

    if (cmdSuccess)
    {
      data[F("DP_TARGET")] = DP_TARGET;
      data[F("DP_PRESS")] = DP_PRESS;
    }
  }

  if (!cmdProcessed && cmd == F("GET TIME"))
  {
    cmdProcessed = true;

    char STOVE_DATETIME[20];
    byte STOVE_WDAY;
    cmdSuccess = _Pala.getDateTime(&STOVE_DATETIME, &STOVE_WDAY);

    if (cmdSuccess)
    {
      data[F("STOVE_DATETIME")] = STOVE_DATETIME;
      data[F("STOVE_WDAY")] = STOVE_WDAY;
    }
  }

  if (!cmdProcessed && cmd == F("GET IOPT"))
  {
    cmdProcessed = true;

    byte IN_I01, IN_I02, IN_I03, IN_I04;
    byte OUT_O01, OUT_O02, OUT_O03, OUT_O04, OUT_O05, OUT_O06, OUT_O07;
    cmdSuccess = _Pala.getIO(&IN_I01, &IN_I02, &IN_I03, &IN_I04, &OUT_O01, &OUT_O02, &OUT_O03, &OUT_O04, &OUT_O05, &OUT_O06, &OUT_O07);

    if (cmdSuccess)
    {
      data[F("IN_I01")] = IN_I01;
      data[F("IN_I02")] = IN_I02;
      data[F("IN_I03")] = IN_I03;
      data[F("IN_I04")] = IN_I04;
      data[F("OUT_O01")] = OUT_O01;
      data[F("OUT_O02")] = OUT_O02;
      data[F("OUT_O03")] = OUT_O03;
      data[F("OUT_O04")] = OUT_O04;
      data[F("OUT_O05")] = OUT_O05;
      data[F("OUT_O06")] = OUT_O06;
      data[F("OUT_O07")] = OUT_O07;
    }
  }

  if (!cmdProcessed && cmd == F("GET SERN"))
  {
    cmdProcessed = true;

    char SN[28];
    cmdSuccess = _Pala.getSN(&SN);

    if (cmdSuccess)
    {
      data[F("SN")] = SN;
    }
  }

  if (!cmdProcessed && cmd == F("GET MDVE"))
  {
    cmdProcessed = true;

    uint16_t MOD, VER, CORE;
    char FWDATE[11];
    cmdSuccess = _Pala.getModelVersion(&MOD, &VER, &CORE, &FWDATE);

    if (cmdSuccess)
    {
      data[F("MOD")] = MOD;
      data[F("VER")] = VER;
      data[F("CORE")] = CORE;
      data[F("FWDATE")] = FWDATE;
    }
  }

  if (!cmdProcessed && cmd == F("GET CHRD"))
  {
    cmdProcessed = true;

    byte CHRSTATUS;
    float PCHRSETP[6];
    byte PSTART[6][2];
    byte PSTOP[6][2];
    byte DM[7][3];
    cmdSuccess = _Pala.getChronoData(&CHRSTATUS, &PCHRSETP, &PSTART, &PSTOP, &DM);

    if (cmdSuccess)
    {
      data[F("CHRSTATUS")] = CHRSTATUS;

      // Add Programs (P1->P6)
      char programName[3] = {'P', 'X', 0};
      char time[6] = {'0', '0', ':', '0', '0', 0};
      for (byte i = 0; i < 6; i++)
      {
        programName[1] = i + '1';
        JsonObject px = data.createNestedObject(programName);
        px[F("CHRSETP")] = serialized(String(PCHRSETP[i], 2));
        time[0] = PSTART[i][0] / 10 + '0';
        time[1] = PSTART[i][0] % 10 + '0';
        time[3] = PSTART[i][1] / 10 + '0';
        time[4] = PSTART[i][1] % 10 + '0';
        px[F("START")] = time;
        time[0] = PSTOP[i][0] / 10 + '0';
        time[1] = PSTOP[i][0] % 10 + '0';
        time[3] = PSTOP[i][1] / 10 + '0';
        time[4] = PSTOP[i][1] % 10 + '0';
        px[F("STOP")] = time;
      }

      // Add Days (D1->D7)
      char dayName[3] = {'D', 'X', 0};
      char memoryName[3] = {'M', 'X', 0};
      for (byte dayNumber = 0; dayNumber < 7; dayNumber++)
      {
        dayName[1] = dayNumber + '1';
        JsonObject dx = data.createNestedObject(dayName);
        for (byte memoryNumber = 0; memoryNumber < 3; memoryNumber++)
        {
          memoryName[1] = memoryNumber + '1';
          if (DM[dayNumber][memoryNumber])
          {
            programName[1] = DM[dayNumber][memoryNumber] + '0';
            dx[memoryName] = programName;
          }
          else
            dx[memoryName] = F("OFF");
        }
      }
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("GET PARM ")))
  {
    cmdProcessed = true;

    String strParamNumber(cmd.substring(9));

    byte paramNumber = strParamNumber.toInt();

    if (paramNumber == 0 && strParamNumber[0] != '0')
    {
      info[F("CMD")] = F("GET PARM");
      info[F("MSG")] = String(F("Incorrect Parameter Number : ")) + strParamNumber;
    }

    if (info[F("MSG")].isNull())
    {
      byte paramValue;
      cmdSuccess = _Pala.getParameter(paramNumber, &paramValue);

      if (cmdSuccess)
      {
        data[F("PAR")] = paramValue;
      }
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("GET HPAR ")))
  {
    cmdProcessed = true;

    String strHiddenParamNumber(cmd.substring(9));

    byte hiddenParamNumber = strHiddenParamNumber.toInt();

    if (hiddenParamNumber == 0 && strHiddenParamNumber[0] != '0')
    {
      info[F("CMD")] = F("GET HPAR");
      info[F("MSG")] = String(F("Incorrect Hidden Parameter Number : ")) + strHiddenParamNumber;
    }

    if (info[F("MSG")].isNull())
    {
      uint16_t hiddenParamValue;
      cmdSuccess = _Pala.getHiddenParameter(hiddenParamNumber, &hiddenParamValue);

      if (cmdSuccess)
      {
        data[F("HPAR")] = hiddenParamValue;
      }
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("CMD ")))
  {
    cmdProcessed = true;

    String strOrder(cmd.substring(4));

    if (strOrder != F("ON") && strOrder != F("OFF"))
    {
      info[F("CMD")] = F("CMD");
      info[F("MSG")] = String(F("Incorrect ON/OFF value : ")) + cmd.substring(4);
    }

    if (info[F("MSG")].isNull())
    {
      if (strOrder == F("ON"))
        cmdSuccess = _Pala.switchOn();
      else if (strOrder == F("OFF"))
        cmdSuccess = _Pala.switchOff();
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET POWR ")))
  {
    cmdProcessed = true;

    byte powerLevel = cmd.substring(9).toInt();

    if (powerLevel == 0)
    {
      info[F("CMD")] = F("SET POWR");
      info[F("MSG")] = String(F("Incorrect Power value : ")) + cmd.substring(9);
    }

    if (info[F("MSG")].isNull())
    {
      byte PWRReturn;
      bool isF2LReturnValid;
      uint16_t _F2LReturn;
      uint16_t FANLMINMAXReturn[6];
      cmdSuccess = _Pala.setPower(powerLevel, &PWRReturn, &isF2LReturnValid, &_F2LReturn, &FANLMINMAXReturn);

      if (cmdSuccess)
      {
        data[F("PWR")] = PWRReturn;
        if (isF2LReturnValid)
          data[F("F2L")] = _F2LReturn;
        JsonArray fanlminmax = data.createNestedArray(F("FANLMINMAX"));
        fanlminmax.add(FANLMINMAXReturn[0]);
        fanlminmax.add(FANLMINMAXReturn[1]);
        fanlminmax.add(FANLMINMAXReturn[2]);
        fanlminmax.add(FANLMINMAXReturn[3]);
        fanlminmax.add(FANLMINMAXReturn[4]);
        fanlminmax.add(FANLMINMAXReturn[5]);
      }
    }
  }

  if (!cmdProcessed && cmd == F("SET PWRU"))
  {
    cmdProcessed = true;

    byte PWRReturn;
    bool isF2LReturnValid;
    uint16_t _F2LReturn;
    uint16_t FANLMINMAXReturn[6];
    cmdSuccess = _Pala.setPowerUp(&PWRReturn, &isF2LReturnValid, &_F2LReturn, &FANLMINMAXReturn);

    if (cmdSuccess)
    {
      data[F("PWR")] = PWRReturn;
      if (isF2LReturnValid)
        data[F("F2L")] = _F2LReturn;
      JsonArray fanlminmax = data.createNestedArray(F("FANLMINMAX"));
      fanlminmax.add(FANLMINMAXReturn[0]);
      fanlminmax.add(FANLMINMAXReturn[1]);
      fanlminmax.add(FANLMINMAXReturn[2]);
      fanlminmax.add(FANLMINMAXReturn[3]);
      fanlminmax.add(FANLMINMAXReturn[4]);
      fanlminmax.add(FANLMINMAXReturn[5]);
    }
  }

  if (!cmdProcessed && cmd == F("SET PWRD"))
  {
    cmdProcessed = true;

    byte PWRReturn;
    bool isF2LReturnValid;
    uint16_t _F2LReturn;
    uint16_t FANLMINMAXReturn[6];
    cmdSuccess = _Pala.setPowerDown(&PWRReturn, &isF2LReturnValid, &_F2LReturn, &FANLMINMAXReturn);

    if (cmdSuccess)
    {
      data[F("PWR")] = PWRReturn;
      if (isF2LReturnValid)
        data[F("F2L")] = _F2LReturn;
      JsonArray fanlminmax = data.createNestedArray(F("FANLMINMAX"));
      fanlminmax.add(FANLMINMAXReturn[0]);
      fanlminmax.add(FANLMINMAXReturn[1]);
      fanlminmax.add(FANLMINMAXReturn[2]);
      fanlminmax.add(FANLMINMAXReturn[3]);
      fanlminmax.add(FANLMINMAXReturn[4]);
      fanlminmax.add(FANLMINMAXReturn[5]);
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET TIME ")))
  {
    cmdProcessed = true;

    String workingCmd(cmd);
    byte posInWorkingCmd = 9;
    String strParams[6];
    uint16_t params[6];
    const __FlashStringHelper *errorMessage[6] = {F("Year"), F("Month"), F("Day"), F("Hour"), F("Minute"), F("Second")};

    // Check format
    if (cmd.length() != 28 || cmd[13] != '-' || cmd[16] != '-' || cmd[19] != ' ' || cmd[22] != ':' || cmd[25] != ':')
    {
      info[F("MSG")] = F("Incorrect DateTime format");
    }

    // replace '-' and ':' by ' '
    workingCmd.replace('-', ' ');
    workingCmd.replace(':', ' ');

    // parse parameters
    for (byte i = 0; i < 6 && info[F("MSG")].isNull(); i++)
    {
      strParams[i] = workingCmd.substring(posInWorkingCmd, workingCmd.indexOf(' ', posInWorkingCmd));
      params[i] = strParams[i].toInt();
      if (params[i] == 0 && strParams[i][0] != '0')
        info[F("MSG")] = String(F("Incorrect ")) + errorMessage[i] + F(" : ") + strParams[i];

      posInWorkingCmd += strParams[i].length() + 1;
    }

    // Check if date is valid
    // basic control
    if (params[0] < 2000 || params[0] > 2099)
      info[F("MSG")] = F("Incorrect Year");
    else if (params[1] < 1 || params[1] > 12)
      info[F("MSG")] = F("Incorrect Month");
    else if ((params[2] < 1 || params[2] > 31) ||
             ((params[2] == 4 || params[2] == 6 || params[2] == 9 || params[2] == 11) && params[3] > 30) ||                        // 30 days month control
             (params[2] == 2 && params[3] > 29) ||                                                                                 // February leap year control
             (params[2] == 2 && params[3] == 29 && !(((params[0] % 4 == 0) && (params[0] % 100 != 0)) || (params[0] % 400 == 0)))) // February not leap year control
      info[F("MSG")] = F("Incorrect Day");
    else if (params[3] > 23)
      info[F("MSG")] = F("Incorrect Hour");
    else if (params[4] > 59)
      info[F("MSG")] = F("Incorrect Minute");
    else if (params[5] > 59)
      info[F("MSG")] = F("Incorrect Second");

    if (info[F("MSG")].isNull())
    {
      char STOVE_DATETIMEReturn[20];
      byte STOVE_WDAYReturn;
      cmdSuccess = _Pala.setDateTime(params[0], params[1], params[2], params[3], params[4], params[5], &STOVE_DATETIMEReturn, &STOVE_WDAYReturn);

      if (cmdSuccess)
      {
        data[F("STOVE_DATETIME")] = STOVE_DATETIMEReturn;
        data[F("STOVE_WDAY")] = STOVE_WDAYReturn;
      }
    }
    else
      info[F("CMD")] = F("SET TIME");
  }

  if (!cmdProcessed && cmd.startsWith(F("SET RFAN ")))
  {
    cmdProcessed = true;

    String strRoomFanLevel(cmd.substring(9));

    byte roomFanLevel = strRoomFanLevel.toInt();

    if (roomFanLevel == 0 && strRoomFanLevel[0] != '0')
    {
      info[F("CMD")] = F("SET RFAN");
      info[F("MSG")] = String(F("Incorrect Room Fan value : ")) + strRoomFanLevel;
    }

    if (info[F("MSG")].isNull())
    {
      bool isPWRReturnValid;
      byte PWRReturn;
      uint16_t F2LReturn;
      uint16_t F2LFReturn;
      cmdSuccess = _Pala.setRoomFan(roomFanLevel, &isPWRReturnValid, &PWRReturn, &F2LReturn, &F2LFReturn);

      if (cmdSuccess)
      {
        if (isPWRReturnValid)
          data[F("PWR")] = PWRReturn;
        data[F("F2L")] = F2LReturn;
        data[F("F2LF")] = F2LFReturn;
      }
    }
  }

  if (!cmdProcessed && cmd == F("SET FN2U"))
  {
    cmdProcessed = true;

    bool isPWRReturnValid;
    byte PWRReturn;
    uint16_t F2LReturn;
    uint16_t F2LFReturn;
    cmdSuccess = _Pala.setRoomFanUp(&isPWRReturnValid, &PWRReturn, &F2LReturn, &F2LFReturn);

    if (cmdSuccess)
    {
      if (isPWRReturnValid)
        data[F("PWR")] = PWRReturn;
      data[F("F2L")] = F2LReturn;
      data[F("F2LF")] = F2LFReturn;
    }
  }

  if (!cmdProcessed && cmd == F("SET FN2D"))
  {
    cmdProcessed = true;

    bool isPWRReturnValid;
    byte PWRReturn;
    uint16_t F2LReturn;
    uint16_t F2LFReturn;
    cmdSuccess = _Pala.setRoomFanDown(&isPWRReturnValid, &PWRReturn, &F2LReturn, &F2LFReturn);

    if (cmdSuccess)
    {
      if (isPWRReturnValid)
        data[F("PWR")] = PWRReturn;
      data[F("F2L")] = F2LReturn;
      data[F("F2LF")] = F2LFReturn;
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET FN3L ")))
  {
    cmdProcessed = true;

    String strRoomFan3Level(cmd.substring(9));

    byte roomFan3Level = strRoomFan3Level.toInt();

    if (roomFan3Level == 0 && strRoomFan3Level[0] != '0')
    {
      info[F("CMD")] = F("SET FN3L");
      info[F("MSG")] = String(F("Incorrect Room Fan 3 value : ")) + strRoomFan3Level;
    }

    if (info[F("MSG")].isNull())
    {
      uint16_t F3LReturn;
      cmdSuccess = _Pala.setRoomFan3(roomFan3Level, &F3LReturn);

      if (cmdSuccess)
      {
        data[F("F3L")] = F3LReturn;
      }
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET FN4L ")))
  {
    cmdProcessed = true;

    String strRoomFan4Level(cmd.substring(9));

    byte roomFan4Level = strRoomFan4Level.toInt();

    if (roomFan4Level == 0 && strRoomFan4Level[0] != '0')
    {
      info[F("CMD")] = F("SET FN4L");
      info[F("MSG")] = String(F("Incorrect Room Fan 4 value : ")) + strRoomFan4Level;
    }

    if (info[F("MSG")].isNull())
    {
      uint16_t F4LReturn;
      cmdSuccess = _Pala.setRoomFan4(roomFan4Level, &F4LReturn);

      if (cmdSuccess)
      {
        data[F("F4L")] = F4LReturn;
      }
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET SLNT ")))
  {
    cmdProcessed = true;

    String strSilentMode = cmd.substring(9);

    byte silentMode = strSilentMode.toInt();

    if (silentMode == 0 && strSilentMode[0] != '0')
    {
      info[F("CMD")] = F("SET SLNT");
      info[F("MSG")] = String(F("Incorrect Silent Mode value : ")) + strSilentMode;
    }

    if (info[F("MSG")].isNull())
    {
      byte SLNTReturn;
      byte PWRReturn;
      uint16_t F2LReturn;
      uint16_t F2LFReturn;
      bool isF3LF4LReturnValid;
      uint16_t F3LReturn;
      uint16_t F4LReturn;
      cmdSuccess = _Pala.setSilentMode(silentMode, &SLNTReturn, &PWRReturn, &F2LReturn, &F2LFReturn, &isF3LF4LReturnValid, &F3LReturn, &F4LReturn);

      if (cmdSuccess)
      {
        data[F("SLNT")] = SLNTReturn;
        data[F("PWR")] = PWRReturn;
        data[F("F2L")] = F2LReturn;
        data[F("F2LF")] = F2LFReturn;
        if (isF3LF4LReturnValid)
        {
          data[F("F3L")] = F3LReturn;
          data[F("F4L")] = F4LReturn;
        }
      }
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET CSST ")))
  {
    cmdProcessed = true;

    String strChronoStatus = cmd.substring(9);

    bool chronoStatus = (strChronoStatus.toInt() != 0);

    if (!chronoStatus && strChronoStatus[0] != '0')
    {
      info[F("CMD")] = F("SET CSST");
      info[F("MSG")] = String(F("Incorrect Chrono Status value : ")) + strChronoStatus;
    }

    if (info[F("MSG")].isNull())
    {
      byte CHRSTATUSReturn;
      cmdSuccess = _Pala.setChronoStatus(chronoStatus, &CHRSTATUSReturn);

      if (cmdSuccess)
      {
        data[F("CHRSTATUS")] = CHRSTATUSReturn;
      }
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET CSTH ")))
  {
    cmdProcessed = true;

    String strProgramNumber(cmd.substring(9, cmd.indexOf(' ', 9)));
    String strStartHour(cmd.substring(cmd.indexOf(' ', 9) + 1));

    byte programNumber = strProgramNumber.toInt();
    byte startHour = strStartHour.toInt();

    if (programNumber == 0 && strProgramNumber[0] != '0')
    {
      info[F("CMD")] = F("SET CSTH");
      info[F("MSG")] = String(F("Incorrect Program Number : ")) + strProgramNumber;
    }

    if (info[F("MSG")].isNull() && startHour == 0 && strStartHour[0] != '0')
    {
      info[F("CMD")] = F("SET CSTH");
      info[F("MSG")] = String(F("Incorrect Start Hour : ")) + strStartHour;
    }

    if (info[F("MSG")].isNull())
    {
      cmdSuccess = _Pala.setChronoStartHH(programNumber, startHour);
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET CSTM ")))
  {
    cmdProcessed = true;

    String strProgramNumber(cmd.substring(9, cmd.indexOf(' ', 9)));
    String strStartMinute(cmd.substring(cmd.indexOf(' ', 9) + 1));

    byte programNumber = strProgramNumber.toInt();
    byte startMinute = strStartMinute.toInt();

    if (programNumber == 0 && strProgramNumber[0] != '0')
    {
      info[F("CMD")] = F("SET CSTM");
      info[F("MSG")] = String(F("Incorrect Program Number : ")) + strProgramNumber;
    }

    if (info[F("MSG")].isNull() && startMinute == 0 && strStartMinute[0] != '0')
    {
      info[F("CMD")] = F("SET CSTM");
      info[F("MSG")] = String(F("Incorrect Start Minute : ")) + startMinute;
    }

    if (info[F("MSG")].isNull())
    {
      cmdSuccess = _Pala.setChronoStartMM(programNumber, startMinute);
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET CSPH ")))
  {
    cmdProcessed = true;

    String strProgramNumber(cmd.substring(9, cmd.indexOf(' ', 9)));
    String strStopHour(cmd.substring(cmd.indexOf(' ', 9) + 1));

    byte programNumber = strProgramNumber.toInt();
    byte stopHour = strStopHour.toInt();

    if (programNumber == 0 && strProgramNumber[0] != '0')
    {

      info[F("CMD")] = F("SET CSPH");
      info[F("MSG")] = String(F("Incorrect Program Number : ")) + strProgramNumber;
    }

    if (info[F("MSG")].isNull() && stopHour == 0 && strStopHour[0] != '0')
    {
      info[F("CMD")] = F("SET CSPH");
      info[F("MSG")] = String(F("Incorrect Stop Hour : ")) + strStopHour;
    }

    if (info[F("MSG")].isNull())
    {
      cmdSuccess = _Pala.setChronoStopHH(programNumber, stopHour);
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET CSPM ")))
  {
    cmdProcessed = true;

    String strProgramNumber(cmd.substring(9, cmd.indexOf(' ', 9)));
    String strStopMinute(cmd.substring(cmd.indexOf(' ', 9) + 1));

    byte programNumber = strProgramNumber.toInt();
    byte stopMinute = strStopMinute.toInt();

    if (programNumber == 0 && strProgramNumber[0] != '0')
    {
      info[F("CMD")] = F("SET CSPM");
      info[F("MSG")] = String(F("Incorrect Program Number : ")) + strProgramNumber;
    }

    if (info[F("MSG")].isNull() && stopMinute == 0 && strStopMinute[0] != '0')
    {
      info[F("CMD")] = F("SET CSPM");
      info[F("MSG")] = String(F("Incorrect Stop Minute : ")) + strStopMinute;
    }

    if (info[F("MSG")].isNull())
    {
      cmdSuccess = _Pala.setChronoStopMM(programNumber, stopMinute);
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET CSET ")))
  {
    cmdProcessed = true;

    String strProgramNumber(cmd.substring(9, cmd.indexOf(' ', 9)));
    String strSetPoint(cmd.substring(cmd.indexOf(' ', 9) + 1));

    byte programNumber = strProgramNumber.toInt();
    byte setPoint = strSetPoint.toInt();

    if (programNumber == 0 && strProgramNumber[0] != '0')
    {
      info[F("CMD")] = F("SET CSET");
      info[F("MSG")] = String(F("Incorrect Program Number : ")) + strProgramNumber;
    }

    if (info[F("MSG")].isNull() && setPoint == 0 && strSetPoint[0] != '0')
    {
      info[F("CMD")] = F("SET CSET");
      info[F("MSG")] = String(F("Incorrect SetPoint : ")) + strSetPoint;
    }

    if (info[F("MSG")].isNull())
    {
      cmdSuccess = _Pala.setChronoSetpoint(programNumber, setPoint);
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET CDAY ")))
  {
    cmdProcessed = true;

    // Starting 9 to first space starting 9
    String strDayNumber(cmd.substring(9, cmd.indexOf(' ', 9)));
    // Starting (9 + previous string length + 1) to first space starting (9 + previous string length + 1)
    String strMemoryNumber(cmd.substring(9 + strDayNumber.length() + 1, cmd.indexOf(' ', 9 + strDayNumber.length() + 1)));
    // Starting (9 + previous strings lengths + number of previous strings) to the end
    String strProgramNumber(cmd.substring(9 + strDayNumber.length() + strMemoryNumber.length() + 2));

    byte dayNumber = strDayNumber.toInt();
    byte memoryNumber = strMemoryNumber.toInt();
    byte programNumber = strProgramNumber.toInt();

    if (dayNumber == 0 && strDayNumber[0] != '0')
    {
      info[F("CMD")] = F("SET CDAY");
      info[F("MSG")] = String(F("Incorrect Day Number : ")) + strDayNumber;
    }

    if (info[F("MSG")].isNull() && memoryNumber == 0 && strMemoryNumber[0] != '0')
    {
      info[F("CMD")] = F("SET CDAY");
      info[F("MSG")] = String(F("Incorrect Memory Number : ")) + strMemoryNumber;
    }

    if (info[F("MSG")].isNull() && programNumber == 0 && strProgramNumber[0] != '0')
    {
      info[F("CMD")] = F("SET CDAY");
      info[F("MSG")] = String(F("Incorrect Program Number : ")) + strProgramNumber;
    }

    if (info[F("MSG")].isNull())
    {
      cmdSuccess = _Pala.setChronoDay(dayNumber, memoryNumber, programNumber);

      if (cmdSuccess)
      {
        char dayName[3] = {'D', 'X', 0};
        char memoryName[3] = {'M', 'X', 0};
        char programName[3] = {'P', 'X', 0};

        dayName[1] = dayNumber + '0';
        memoryName[1] = memoryNumber + '0';
        programName[1] = programNumber + '0';

        JsonObject dx = data.createNestedObject(dayName);
        if (programNumber)
          dx[memoryName] = programName;
        else
          dx[memoryName] = F("OFF");
      }
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET CPRD ")))
  {
    cmdProcessed = true;

    byte posInCmd = 9;
    String strParams[6];
    byte params[6];
    const __FlashStringHelper *errorMessage[6] = {F("Program Number"), F("SetPoint"), F("Start Hour"), F("Start Minute"), F("Stop Hour"), F("Stop Minute")};

    for (byte i = 0; i < 6 && info[F("MSG")].isNull(); i++)
    {
      strParams[i] = cmd.substring(posInCmd, cmd.indexOf(' ', posInCmd));
      params[i] = strParams[i].toInt();
      if (params[i] == 0 && strParams[i][0] != '0')
      {
        info[F("CMD")] = F("SET CPRD");
        info[F("MSG")] = String(F("Incorrect ")) + errorMessage[i] + F(" : ") + strParams[i];
      }
      posInCmd += strParams[i].length() + 1;
    }

    if (info[F("MSG")].isNull())
    {
      cmdSuccess = _Pala.setChronoPrg(params[0], params[1], params[2], params[3], params[4], params[5]);

      if (cmdSuccess)
      {
        char programName[3] = {'P', 'X', 0};
        char time[6] = {'0', '0', ':', '0', '0', 0};

        programName[1] = params[0] + '0';
        JsonObject px = data.createNestedObject(programName);
        px[F("CHRSETP")] = (float)params[1];
        time[0] = params[2] / 10 + '0';
        time[1] = params[2] % 10 + '0';
        time[3] = params[3] / 10 + '0';
        time[4] = params[3] % 10 + '0';
        px[F("START")] = time;
        time[0] = params[4] / 10 + '0';
        time[1] = params[4] % 10 + '0';
        time[3] = params[5] / 10 + '0';
        time[4] = params[5] % 10 + '0';
        px[F("STOP")] = time;
      }
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET SETP ")))
  {
    cmdProcessed = true;

    byte setPoint = cmd.substring(9).toInt();

    if (setPoint == 0)
    {
      info[F("CMD")] = F("SET SETP");
      info[F("MSG")] = String(F("Incorrect SetPoint value : ")) + cmd.substring(9);
    }

    if (info[F("MSG")].isNull())
    {
      float SETPReturn;
      cmdSuccess = _Pala.setSetpoint(setPoint, &SETPReturn);

      if (cmdSuccess)
      {
        data[F("SETP")] = serialized(String(SETPReturn, 2));
      }
    }
  }

  if (!cmdProcessed && cmd == F("SET STPU"))
  {
    cmdProcessed = true;

    float SETPReturn;
    cmdSuccess = _Pala.setSetPointUp(&SETPReturn);

    if (cmdSuccess)
    {
      data[F("SETP")] = serialized(String(SETPReturn, 2));
    }
  }

  if (!cmdProcessed && cmd == F("SET STPD"))
  {
    cmdProcessed = true;

    float SETPReturn;
    cmdSuccess = _Pala.setSetPointDown(&SETPReturn);

    if (cmdSuccess)
    {
      data[F("SETP")] = serialized(String(SETPReturn, 2));
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET STPF ")))
  {
    cmdProcessed = true;

    float setPointFloat = cmd.substring(9).toFloat();

    if (setPointFloat == 0.0f)
    {
      info[F("CMD")] = F("SET STPF");
      info[F("MSG")] = String(F("Incorrect SetPoint Float value : ")) + cmd.substring(9);
    }

    if (info[F("MSG")].isNull())
    {
      float SETPReturn;
      cmdSuccess = _Pala.setSetpoint(setPointFloat, &SETPReturn);

      if (cmdSuccess)
      {
        data[F("SETP")] = serialized(String(SETPReturn, 2));
      }
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET PARM ")))
  {
    cmdProcessed = true;

    String strParamNumber(cmd.substring(9, cmd.indexOf(' ', 9)));
    String strParamValue(cmd.substring(cmd.indexOf(' ', 9) + 1));

    byte paramNumber = strParamNumber.toInt();
    byte paramValue = strParamValue.toInt();

    if (paramNumber == 0 && strParamNumber[0] != '0')
    {
      info[F("CMD")] = F("SET PARM");
      info[F("MSG")] = String(F("Incorrect Parameter Number : ")) + strParamNumber;
    }

    if (info[F("MSG")].isNull() && paramValue == 0 && strParamValue[0] != '0')
    {
      info[F("CMD")] = F("SET PARM");
      info[F("MSG")] = String(F("Incorrect Parameter Value : ")) + strParamValue;
    }

    if (info[F("MSG")].isNull())
    {
      cmdSuccess = _Pala.setParameter(paramNumber, paramValue);

      if (cmdSuccess)
      {
        data[String(F("PAR")) + paramNumber] = paramValue;
      }
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET HPAR ")))
  {
    cmdProcessed = true;

    String strHiddenParamNumber(cmd.substring(9, cmd.indexOf(' ', 9)));
    String strHiddenParamValue(cmd.substring(cmd.indexOf(' ', 9) + 1));

    byte hiddenParamNumber = strHiddenParamNumber.toInt();
    uint16_t hiddenParamValue = strHiddenParamValue.toInt();

    if (hiddenParamNumber == 0 && strHiddenParamNumber[0] != '0')
    {
      info[F("CMD")] = F("SET HPAR");
      info[F("MSG")] = String(F("Incorrect Hidden Parameter Number : ")) + strHiddenParamNumber;
    }

    if (info[F("MSG")].isNull() && hiddenParamValue == 0 && strHiddenParamValue[0] != '0')
    {
      info[F("CMD")] = F("SET HPAR");
      info[F("MSG")] = String(F("Incorrect Hidden Parameter Value : ")) + strHiddenParamValue;
    }

    if (info[F("MSG")].isNull())
    {
      cmdSuccess = _Pala.setHiddenParameter(hiddenParamNumber, hiddenParamValue);

      if (cmdSuccess)
      {
        data[String(F("HPAR")) + hiddenParamNumber] = hiddenParamValue;
      }
    }
  }

  // if command has been processed
  if (cmdProcessed)
  {
    info[F("CMD")] = cmd;
    // successfully
    if (cmdSuccess)
    {
      info[F("RSP")] = F("OK");
      jsonDoc[F("SUCCESS")] = true;
      String strData;
      serializeJson(data, strData);
      statusEventSourceBroadcast(strData);
      return true;
    }
    else
    {
      // if there is no MSG in info then stove communication failed
      if (info[F("MSG")].isNull())
      {
        info[F("RSP")] = F("TIMEOUT");
        info[F("MSG")] = F("Stove communication failed");
      }
      else
        info[F("RSP")] = F("ERROR");

      jsonDoc[F("SUCCESS")] = false;
      data[F("NODATA")] = true;
    }
  }
  else
  {
    // command is unknown and not processed
    info[F("RSP")] = F("ERROR");
    info[F("CMD")] = F("UNKNOWN");
    info[F("MSG")] = F("No valid request received");
    jsonDoc[F("SUCCESS")] = false;
    data[F("NODATA")] = true;
  }

  return false;
}

void WebPalaControl::udpRequestHandler(WiFiUDP &udpServer)
{

  int packetSize = udpServer.parsePacket();
  if (packetSize <= 0)
    return;

  String strData;
  strData.reserve(packetSize + 1);
  DynamicJsonDocument jsonDoc(2048);

  // while udpServer.read() do not return -1, get returned value and add it to strData
  int bufferByte;
  while ((bufferByte = udpServer.read()) >= 0)
    strData += (char)bufferByte;

  // process request
  if (strData.endsWith(F("bridge?")))
    executePalaCmd(F("GET STDT"), jsonDoc);
  else if (strData.endsWith(F("bridge?GET ALLS")))
    executePalaCmd(F("GET ALLS"), jsonDoc);
  else
    executePalaCmd("", jsonDoc);

  String strAnswer;
  serializeJson(jsonDoc, strAnswer); // serialize resturned JSON as-is

  // answer to the requester
  udpServer.beginPacket(udpServer.remoteIP(), udpServer.remotePort());
  udpServer.write(strAnswer.c_str(), strAnswer.length());
  udpServer.endPacket();
}

//------------------------------------------
// Used to initialize configuration properties to default values
void WebPalaControl::setConfigDefaultValues()
{
  _ha.protocol = HA_PROTO_DISABLED;
  _ha.hostname[0] = 0;
  _ha.uploadPeriod = 60;

  _ha.mqtt.type = HA_MQTT_GENERIC_JSON;
  _ha.mqtt.port = 1883;
  _ha.mqtt.username[0] = 0;
  _ha.mqtt.password[0] = 0;
  strcpy_P(_ha.mqtt.generic.baseTopic, PSTR("$model$"));
}

//------------------------------------------
// Parse JSON object into configuration properties
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
}

//------------------------------------------
// Parse HTTP POST parameters in request into configuration properties
bool WebPalaControl::parseConfigWebRequest(ESP8266WebServer &server)
{

  // Parse HA protocol
  if (server.hasArg(F("haproto")))
    _ha.protocol = server.arg(F("haproto")).toInt();

  // if an home Automation protocol has been selected then get common param
  if (_ha.protocol != HA_PROTO_DISABLED)
  {
    if (server.hasArg(F("hahost")) && server.arg(F("hahost")).length() < sizeof(_ha.hostname))
      strcpy(_ha.hostname, server.arg(F("hahost")).c_str());
    if (server.hasArg(F("haupperiod")))
      _ha.uploadPeriod = server.arg(F("haupperiod")).toInt();
  }

  // Now get specific param
  switch (_ha.protocol)
  {

  case HA_PROTO_MQTT:

    if (server.hasArg(F("hamtype")))
      _ha.mqtt.type = server.arg(F("hamtype")).toInt();
    if (server.hasArg(F("hamport")))
      _ha.mqtt.port = server.arg(F("hamport")).toInt();
    if (server.hasArg(F("hamu")) && server.arg(F("hamu")).length() < sizeof(_ha.mqtt.username))
      strcpy(_ha.mqtt.username, server.arg(F("hamu")).c_str());
    char tempPassword[64 + 1] = {0};
    // put MQTT password into temporary one for predefpassword
    if (server.hasArg(F("hamp")) && server.arg(F("hamp")).length() < sizeof(tempPassword))
      strcpy(tempPassword, server.arg(F("hamp")).c_str());
    // check for previous password (there is a predefined special password that mean to keep already saved one)
    if (strcmp_P(tempPassword, appDataPredefPassword))
      strcpy(_ha.mqtt.password, tempPassword);

    switch (_ha.mqtt.type)
    {
    case HA_MQTT_GENERIC:
    case HA_MQTT_GENERIC_JSON:
    case HA_MQTT_GENERIC_CATEGORIZED:
      if (server.hasArg(F("hamgbt")) && server.arg(F("hamgbt")).length() < sizeof(_ha.mqtt.generic.baseTopic))
        strcpy(_ha.mqtt.generic.baseTopic, server.arg(F("hamgbt")).c_str());

      if (!_ha.hostname[0] || !_ha.mqtt.generic.baseTopic[0])
        _ha.protocol = HA_PROTO_DISABLED;
      break;
    }
    break;
  }
  return true;
}

//------------------------------------------
// Generate JSON from configuration properties
String WebPalaControl::generateConfigJSON(bool forSaveFile = false)
{
  String gc('{');

  gc = gc + F("\"haproto\":") + _ha.protocol;
  gc = gc + F(",\"hahost\":\"") + _ha.hostname + '"';
  gc = gc + F(",\"haupperiod\":") + _ha.uploadPeriod;

  // if for WebPage or protocol selected is MQTT
  if (!forSaveFile || _ha.protocol == HA_PROTO_MQTT)
  {
    gc = gc + F(",\"hamtype\":") + _ha.mqtt.type;
    gc = gc + F(",\"hamport\":") + _ha.mqtt.port;
    gc = gc + F(",\"hamu\":\"") + _ha.mqtt.username + '"';
    if (forSaveFile)
      gc = gc + F(",\"hamp\":\"") + _ha.mqtt.password + '"';
    else
      gc = gc + F(",\"hamp\":\"") + (__FlashStringHelper *)appDataPredefPassword + '"'; // predefined special password (mean to keep already saved one)

    gc = gc + F(",\"hamgbt\":\"") + _ha.mqtt.generic.baseTopic + '"';
  }

  gc += '}';

  return gc;
}

//------------------------------------------
// Generate JSON of application status
String WebPalaControl::generateStatusJSON()
{
  String gs('{');

  char SN[28];
  if (_Pala.getSN(&SN))
    gs = gs + F("\"liveData\":{\"SN\":\"") + SN + F("\"}");
  else
    gs = gs + F("\"liveData\":{\"MSG\":\"Stove communication failed! please check cabling to your stove.\"}");

  gs = gs + F(",\"has1\":\"");
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
}

//------------------------------------------
// code to execute during initialization and reinitialization of the app
bool WebPalaControl::appInit(bool reInit)
{
  // Stop UdpServer
  _udpServer.stop();

  // Stop Publish
  _publishTicker.detach();

  // Stop MQTT
  _mqttMan.disconnect();

  // if MQTT used so configure it
  if (_ha.protocol == HA_PROTO_MQTT)
  {
    // prepare will topic
    String willTopic = _ha.mqtt.generic.baseTopic;
    MQTTMan::prepareTopic(willTopic);
    willTopic += F("connected");

    // setup MQTT
    _mqttMan.setClient(_wifiClient).setServer(_ha.hostname, _ha.mqtt.port);
    _mqttMan.setConnectedAndWillTopic(willTopic.c_str());
    _mqttMan.setConnectedCallback(std::bind(&WebPalaControl::mqttConnectedCallback, this, std::placeholders::_1, std::placeholders::_2));
    _mqttMan.setCallback(std::bind(&WebPalaControl::mqttCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // Connect
    _mqttMan.connect(_ha.mqtt.username, _ha.mqtt.password);
  }

  LOG_SERIAL.println(F("Connecting to Stove..."));

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

  if (res)
  {
    LOG_SERIAL.println(F("Stove connected"));
    char SN[28];
    _Pala.getSN(&SN);
    LOG_SERIAL.printf("Stove Serial Number: %s", SN);
  }
  else
  {
    LOG_SERIAL.println(F("Stove connection failed"));
  }

  if (res)
    publishTick(); // if configuration changed, publish immediately
  _publishTicker.attach(_ha.uploadPeriod, [this]()
                        { this->_needPublish = true; });

  // Start UDP Server
  _udpServer.begin(54549);

  return res;
}

//------------------------------------------
// Return HTML Code to insert into Status Web page
const PROGMEM char *WebPalaControl::getHTMLContent(WebPageForPlaceHolder wp)
{
  switch (wp)
  {
  case status:
    return status1htmlgz;
    break;
  case config:
    return config1htmlgz;
    break;
  default:
    return nullptr;
    break;
  };
  return nullptr;
}

// and his Size
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
}

//------------------------------------------
// code to register web request answer to the web server
void WebPalaControl::appInitWebServer(ESP8266WebServer &server, bool &shouldReboot, bool &pauseApplication)
{
  // Handle HTTP GET requests
  server.on("/cgi-bin/sendmsg.lua", HTTP_GET, [this, &server]()
            {
    String cmd;
    DynamicJsonDocument jsonDoc(2048);

    if (server.hasArg(F("cmd"))) cmd = server.arg(F("cmd"));

    // WPalaControl specific command
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
        String ret(F("{\"INFO\":{\"CMD\":\"BKP PARM\",\"MSG\":\"Incorrect File Type : "));
        ret += strFileType;
        ret += F("\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}");
        server.send(200, F("text/json"), ret);
        return;
      }

      byte params[0x6A];
      res &= _Pala.getAllParameters(&params);

      if (res)
      {
        String toReturn;

        switch (fileType)
        {
        case 0: //CSV
          toReturn += F("PARM;VALUE\r\n");
          for (byte i = 0; i < 0x6A; i++)
            toReturn += String(i) + ';' + params[i] + '\r' + '\n';

          server.sendHeader(F("Content-Disposition"), F("attachment; filename=\"PARM.csv\""));
          server.send(200, F("text/csv"), toReturn);
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

          server.sendHeader(F("Content-Disposition"), F("attachment; filename=\"PARM.json\""));
          server.send(200, F("text/json"), toReturn);
          break;
        }

        return;
      }
      else
      {
        server.send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"BKP PARM\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
        return;
      }
    }

    // WPalaControl specific command
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
        String ret(F("{\"INFO\":{\"CMD\":\"BKP HPAR\",\"MSG\":\"Incorrect File Type : "));
        ret += strFileType;
        ret += F("\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}");
        server.send(200, F("text/json"), ret);
        return;
      }

      uint16_t hiddenParams[0x6F];
      res &= _Pala.getAllHiddenParameters(&hiddenParams);

      if (res)
      {
        String toReturn;

        switch (fileType)
        {
        case 0: //CSV
          toReturn += F("HPAR;VALUE\r\n");
          for (byte i = 0; i < 0x6F; i++)
            toReturn += String(i) + ';' + hiddenParams[i] + '\r' + '\n';

          server.sendHeader(F("Content-Disposition"), F("attachment; filename=\"HPAR.csv\""));
          server.send(200, F("text/csv"), toReturn);
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

          server.sendHeader(F("Content-Disposition"), F("attachment; filename=\"HPAR.json\""));
          server.send(200, F("text/json"), toReturn);
          break;
        }

        return;
      }
      else
      {
        server.send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"BKP HPAR\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
        return;
      }
    }

    // Other commands processed using normal Palazzetti logic
    executePalaCmd(cmd, jsonDoc);

    // send response
    String toReturn;
    serializeJson(jsonDoc, toReturn); // serialize returned JSON as-is
    server.send(200, F("text/json"), toReturn); });

  // Handle HTTP POST requests (Body contains a JSON)
  server.on(
      "/cgi-bin/sendmsg.lua", HTTP_POST, [this, &server]()
      {
        String cmd;
        DynamicJsonDocument jsonDoc(2048);

        DeserializationError error = deserializeJson(jsonDoc, server.arg(F("plain")));

        if (!error && !jsonDoc[F("command")].isNull())
          cmd = jsonDoc[F("command")].as<String>();


        // clear jsonDoc to reuse it
        jsonDoc.clear();

        // process cmd
        executePalaCmd(cmd, jsonDoc);

        // send response
        String toReturn;
        serializeJson(jsonDoc, toReturn); // serialize returned JSON as-is
        server.send(200, F("text/json"), toReturn); });
}

//------------------------------------------
// Run for timer
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

  // Handle UDP requests
  udpRequestHandler(_udpServer);
}

//------------------------------------------
// Constructor
WebPalaControl::WebPalaControl(char appId, String appName) : Application(appId, appName)
{
  // TX/GPIO15 is pulled down and so block the stove bus by default...
  pinMode(15, OUTPUT); // set TX PIN to OUTPUT HIGH to unlock bus during WiFi connection
  digitalWrite(15, HIGH);
}
