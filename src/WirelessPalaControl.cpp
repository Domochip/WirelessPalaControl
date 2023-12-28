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
  size_t avail;
  esp8266::polledTimeout::oneShotMs timeOut(timeout);
  while ((avail = Serial.available()) == 0 && !timeOut)
    ;

  return avail;
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
  while (Serial.read() != -1)
    ; // flush RX buffer
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
  String strJson;

  // convert payload to String cmd
  cmd.concat((char *)payload, length);

  // replace '+' by ' '
  cmd.replace('+', ' ');

  // execute Palazzetti command
  executePalaCmd(cmd, strJson, true);

  // publish json result to MQTT
  String baseTopic = _ha.mqtt.generic.baseTopic;
  MQTTMan::prepareTopic(baseTopic);
  String resTopic(baseTopic);
  resTopic += F("result");
  _mqttMan.publish(resTopic.c_str(), strJson.c_str());
}

bool WebPalaControl::publishDataToMqtt(const String &baseTopic, const String &palaCategory, const DynamicJsonDocument &jsonDoc)
{
  bool res = false;
  if (_mqttMan.connected())
  {
    if (_ha.mqtt.type == HA_MQTT_GENERIC)
    {
      // for each key/value pair in DATA
      for (JsonPairConst kv : jsonDoc["DATA"].as<JsonObjectConst>())
      {
        // prepare topic
        String topic(baseTopic);
        topic += kv.key().c_str();
        // publish
        res = _mqttMan.publish(topic.c_str(), kv.value().as<String>().c_str());
      }
    }

    if (_ha.mqtt.type == HA_MQTT_GENERIC_JSON)
    {
      // prepare topic
      String topic(baseTopic);
      topic += palaCategory;
      // serialize DATA to JSON
      String serializedData;
      serializeJson(jsonDoc["DATA"], serializedData);
      // publish
      res = _mqttMan.publish(topic.c_str(), serializedData.c_str());
    }

    if (_ha.mqtt.type == HA_MQTT_GENERIC_JSON)
    {
      // prepare category topic
      String categoryTopic(baseTopic);
      categoryTopic += palaCategory;
      categoryTopic += '/';

      // for each key/value pair in DATA
      for (JsonPairConst kv : jsonDoc["DATA"].as<JsonObjectConst>())
      {
        // prepare topic
        String topic(categoryTopic);
        topic += kv.key().c_str();
        // publish
        res = _mqttMan.publish(topic.c_str(), kv.value().as<String>().c_str());
      }
    }
  }
  return res;
}

bool WebPalaControl::executePalaCmd(const String &cmd, String &strJson, bool publish /* = false*/)
{
  bool cmdProcessed = false; // cmd has been processed
  bool cmdSuccess = false;   // Palazzetti function calls successful

  // Prepare answer structure
  DynamicJsonDocument jsonDoc(2048);
  JsonObject info = jsonDoc.createNestedObject("INFO");
  JsonObject data = jsonDoc.createNestedObject("DATA");

  // Parse parameters
  byte cmdParamNumber = 0;
  String strCmdParams[6];
  uint16_t cmdParams[6];
  bool validCmdParams[6];

  if (cmd.length() > 9 && cmd[8] == ' ')
  {
    String cmdWorkingCopy = cmd.substring(9);
    cmdWorkingCopy.trim();

    // SET TIME special case
    if (cmd.startsWith(F("SET TIME ")))
    {
      cmdWorkingCopy.replace('-', ' ');
      cmdWorkingCopy.replace(':', ' ');
    }

    // SET STPF special case
    if (cmd.startsWith(F("SET STPF ")))
      cmdWorkingCopy.replace('.', ' ');

    while (cmdWorkingCopy.length() && cmdParamNumber < 6)
    {
      int pos = cmdWorkingCopy.indexOf(' ');
      if (pos == -1)
      {
        strCmdParams[cmdParamNumber] = cmdWorkingCopy;
        cmdWorkingCopy = "";
      }
      else
      {
        strCmdParams[cmdParamNumber] = cmdWorkingCopy.substring(0, pos);
        cmdWorkingCopy = cmdWorkingCopy.substring(pos + 1);
      }
      cmdParams[cmdParamNumber] = strCmdParams[cmdParamNumber].toInt();

      // verify convertion is successfull
      // ( copy strCmdParams and remove all 0 from the string, if convertion result is 0, then resulting string should be empty)
      String validation = strCmdParams[cmdParamNumber];
      validation.replace("0", "");
      validCmdParams[cmdParamNumber] = (cmdParams[cmdParamNumber] != 0 || validation.length() == 0);

      cmdParamNumber++;
    }

    // too much parameters has been sent
    if (cmdWorkingCopy.length())
    {
      cmdProcessed = true;
      info["MSG"] = F("Incorrect Parameter Number");
    }
  }

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
      data["LABEL"] = WiFi.getHostname();

      // Network infos
      data["GWDEVICE"] = F("wlan0"); // always wifi
      data["MAC"] = WiFi.macAddress();
      data["GATEWAY"] = WiFi.gatewayIP().toString();
      JsonArray dns = data.createNestedArray("DNS");
      dns.add(WiFi.dnsIP().toString());

      // Wifi infos
      data["WMAC"] = WiFi.macAddress();
      data["WMODE"] = (WiFi.getMode() & WIFI_STA) ? F("sta") : F("ap");
      data["WADR"] = (WiFi.getMode() & WIFI_STA) ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
      data["WGW"] = WiFi.gatewayIP().toString();
      data["WENC"] = F("psk2");
      data["WPWR"] = String(WiFi.RSSI()) + F(" dBm"); // need conversion to dBm?
      data["WSSID"] = WiFi.SSID();
      data["WPR"] = (true) ? F("dhcp") : F("static");
      data["WMSK"] = WiFi.subnetMask().toString();
      data["WBCST"] = WiFi.broadcastIP().toString();
      data["WCH"] = String(WiFi.channel());

      // Ethernet infos
      data["EPR"] = F("dhcp");
      data["EGW"] = F("0.0.0.0");
      data["EMSK"] = F("0.0.0.0");
      data["EADR"] = F("0.0.0.0");
      data["EMAC"] = WiFi.macAddress();
      data["ECBL"] = F("down");
      data["EBCST"] = "";

      data["APLCONN"] = 1; // appliance connected
      data["ICONN"] = 0;   // internet connected

      data["CBTYPE"] = F("miniembplug"); // CBox model
      data["sendmsg"] = F("2.1.2 2018-03-28 10:19:09");
      data["plzbridge"] = F("2.2.1 2021-10-08 09:30:45");
      data["SYSTEM"] = F("2.5.3 2021-10-08 10:30:20 (657c8cf)");

      data["CLOUD_ENABLED"] = true;

      // ----- Values from stove -----
      data["SN"] = SN;
      data["SNCHK"] = SNCHK;
      data["MBTYPE"] = MBTYPE;
      data["MOD"] = MOD;
      data["VER"] = VER;
      data["CORE"] = CORE;
      data["FWDATE"] = FWDATE;
      data["FLUID"] = FLUID;
      data["SPLMIN"] = SPLMIN;
      data["SPLMAX"] = SPLMAX;
      data["UICONFIG"] = UICONFIG;
      data["HWTYPE"] = HWTYPE;
      data["DSPFWVER"] = DSPFWVER;
      data["CONFIG"] = CONFIG;
      data["PELLETTYPE"] = PELLETTYPE;
      data["PSENSTYPE"] = PSENSTYPE;
      data["PSENSLMAX"] = PSENSLMAX;
      data["PSENSLTSH"] = PSENSLTSH;
      data["PSENSLMIN"] = PSENSLMIN;
      data["MAINTPROBE"] = MAINTPROBE;
      data["STOVETYPE"] = STOVETYPE;
      data["FAN2TYPE"] = FAN2TYPE;
      data["FAN2MODE"] = FAN2MODE;
      data["BLEMBMODE"] = BLEMBMODE;
      data["BLEDSPMODE"] = BLEDSPMODE;
      data["CHRONOTYPE"] = 0; // disable chronothermostat (no planning) (enabled if > 1)
      data["AUTONOMYTYPE"] = AUTONOMYTYPE;
      data["NOMINALPWR"] = NOMINALPWR;
    }
  }

  if (!cmdProcessed && cmd == F("GET LABL"))
  {
    cmdProcessed = true;
    cmdSuccess = true;

    data["LABEL"] = WiFi.getHostname();
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

      data["MBTYPE"] = MBTYPE;
      data["MAC"] = WiFi.macAddress();
      data["MOD"] = MOD;
      data["VER"] = VER;
      data["CORE"] = CORE;
      data["FWDATE"] = FWDATE;
      data["APLTS"] = APLTS;
      data["APLWDAY"] = APLWDAY;
      data["CHRSTATUS"] = CHRSTATUS;
      data["STATUS"] = STATUS;
      data["LSTATUS"] = LSTATUS;
      if (isMFSTATUSValid)
        data["MFSTATUS"] = MFSTATUS;
      data["SETP"] = serialized(String(SETP, 2));
      data["PUMP"] = PUMP;
      data["PQT"] = PQT;
      data["F1V"] = F1V;
      data["F1RPM"] = F1RPM;
      data["F2L"] = F2L;
      data["F2LF"] = F2LF;
      JsonArray fanlminmax = data.createNestedArray("FANLMINMAX");
      fanlminmax.add(FANLMINMAX[0]);
      fanlminmax.add(FANLMINMAX[1]);
      fanlminmax.add(FANLMINMAX[2]);
      fanlminmax.add(FANLMINMAX[3]);
      fanlminmax.add(FANLMINMAX[4]);
      fanlminmax.add(FANLMINMAX[5]);
      data["F2V"] = F2V;
      if (isF3LF4LValid)
      {
        data["F3L"] = F3L;
        data["F4L"] = F4L;
      }
      data["PWR"] = PWR;
      data["FDR"] = serialized(String(FDR, 2));
      data["DPT"] = DPT;
      data["DP"] = DP;
      data["IN"] = IN;
      data["OUT"] = OUT;
      data["T1"] = serialized(String(T1, 2));
      data["T2"] = serialized(String(T2, 2));
      data["T3"] = serialized(String(T3, 2));
      data["T4"] = serialized(String(T4, 2));
      data["T5"] = serialized(String(T5, 2));

      data["EFLAGS"] = 0; // new ErrorFlags not implemented
      if (isSNValid)
        data["SN"] = SN;
    }
  }

  if (!cmdProcessed && cmd == F("GET STAT"))
  {
    cmdProcessed = true;

    uint16_t STATUS, LSTATUS;
    cmdSuccess = _Pala.getStatus(&STATUS, &LSTATUS);

    if (cmdSuccess)
    {
      data["STATUS"] = STATUS;
      data["LSTATUS"] = LSTATUS;
    }
  }

  if (!cmdProcessed && cmd == F("GET TMPS"))
  {
    cmdProcessed = true;

    float T1, T2, T3, T4, T5;
    cmdSuccess = _Pala.getAllTemps(&T1, &T2, &T3, &T4, &T5);

    if (cmdSuccess)
    {
      data["T1"] = serialized(String(T1, 2));
      data["T2"] = serialized(String(T2, 2));
      data["T3"] = serialized(String(T3, 2));
      data["T4"] = serialized(String(T4, 2));
      data["T5"] = serialized(String(T5, 2));
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
      data["F1V"] = F1V;
      data["F2V"] = F2V;
      data["F1RPM"] = F1RPM;
      data["F2L"] = F2L;
      data["F2LF"] = F2LF;
      if (isF3SF4SValid)
      {
        data["F3S"] = serialized(String(F3S, 2));
        data["F4S"] = serialized(String(F4S, 2));
      }
      if (isF3LF4LValid)
      {
        data["F3L"] = F3L;
        data["F4L"] = F4L;
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
      data["SETP"] = serialized(String(SETP, 2));
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
      data["PWR"] = PWR;
      data["FDR"] = serialized(String(FDR, 2));
    }
  }

  if (!cmdProcessed && (cmd == F("GET CUNT") || cmd == F("GET CNTR")))
  {
    cmdProcessed = true;

    uint16_t IGN, POWERTIMEh, POWERTIMEm, HEATTIMEh, HEATTIMEm, SERVICETIMEh, SERVICETIMEm, ONTIMEh, ONTIMEm, OVERTMPERRORS, IGNERRORS, PQT;
    cmdSuccess = _Pala.getCounters(&IGN, &POWERTIMEh, &POWERTIMEm, &HEATTIMEh, &HEATTIMEm, &SERVICETIMEh, &SERVICETIMEm, &ONTIMEh, &ONTIMEm, &OVERTMPERRORS, &IGNERRORS, &PQT);

    if (cmdSuccess)
    {
      data["IGN"] = IGN;
      data["POWERTIME"] = String(POWERTIMEh) + ':' + (POWERTIMEm / 10) + (POWERTIMEm % 10);
      data["HEATTIME"] = String(HEATTIMEh) + ':' + (HEATTIMEm / 10) + (HEATTIMEm % 10);
      data["SERVICETIME"] = String(SERVICETIMEh) + ':' + (SERVICETIMEm / 10) + (SERVICETIMEm % 10);
      data["ONTIME"] = String(ONTIMEh) + ':' + (ONTIMEm / 10) + (ONTIMEm % 10);
      data["OVERTMPERRORS"] = OVERTMPERRORS;
      data["IGNERRORS"] = IGNERRORS;
      data["PQT"] = PQT;
    }
  }

  if (!cmdProcessed && cmd == F("GET DPRS"))
  {
    cmdProcessed = true;

    uint16_t DP_TARGET, DP_PRESS;
    cmdSuccess = _Pala.getDPressData(&DP_TARGET, &DP_PRESS);

    if (cmdSuccess)
    {
      data["DP_TARGET"] = DP_TARGET;
      data["DP_PRESS"] = DP_PRESS;
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
      data["STOVE_DATETIME"] = STOVE_DATETIME;
      data["STOVE_WDAY"] = STOVE_WDAY;
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
      data["IN_I01"] = IN_I01;
      data["IN_I02"] = IN_I02;
      data["IN_I03"] = IN_I03;
      data["IN_I04"] = IN_I04;
      data["OUT_O01"] = OUT_O01;
      data["OUT_O02"] = OUT_O02;
      data["OUT_O03"] = OUT_O03;
      data["OUT_O04"] = OUT_O04;
      data["OUT_O05"] = OUT_O05;
      data["OUT_O06"] = OUT_O06;
      data["OUT_O07"] = OUT_O07;
    }
  }

  if (!cmdProcessed && cmd == F("GET SERN"))
  {
    cmdProcessed = true;

    char SN[28];
    cmdSuccess = _Pala.getSN(&SN);

    if (cmdSuccess)
    {
      data["SN"] = SN;
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
      data["MOD"] = MOD;
      data["VER"] = VER;
      data["CORE"] = CORE;
      data["FWDATE"] = FWDATE;
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
      data["CHRSTATUS"] = CHRSTATUS;

      // Add Programs (P1->P6)
      char programName[3] = {'P', 'X', 0};
      char time[6] = {'0', '0', ':', '0', '0', 0};
      for (byte i = 0; i < 6; i++)
      {
        programName[1] = i + '1';
        JsonObject px = data.createNestedObject(programName);
        px["CHRSETP"] = serialized(String(PCHRSETP[i], 2));
        time[0] = PSTART[i][0] / 10 + '0';
        time[1] = PSTART[i][0] % 10 + '0';
        time[3] = PSTART[i][1] / 10 + '0';
        time[4] = PSTART[i][1] % 10 + '0';
        px["START"] = time;
        time[0] = PSTOP[i][0] / 10 + '0';
        time[1] = PSTOP[i][0] % 10 + '0';
        time[3] = PSTOP[i][1] / 10 + '0';
        time[4] = PSTOP[i][1] % 10 + '0';
        px["STOP"] = time;
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

    if (cmdParamNumber != 1)
      info["MSG"] = String(F("Incorrect Parameter Number : ")) + cmdParamNumber;
    else if (!validCmdParams[0])
      info["MSG"] = String(F("Incorrect Parameter Value : ")) + strCmdParams[0];

    if (info["MSG"].isNull())
    {
      byte paramValue;
      cmdSuccess = _Pala.getParameter(cmdParams[0], &paramValue);

      if (cmdSuccess)
      {
        String paramName("PAR");
        paramName += cmdParams[0];
        data[paramName] = paramValue;
      }
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("GET HPAR ")))
  {
    cmdProcessed = true;

    if (cmdParamNumber != 1)
      info["MSG"] = String(F("Incorrect Parameter Number : ")) + cmdParamNumber;
    else if (!validCmdParams[0])
      info["MSG"] = String(F("Incorrect Parameter Value : ")) + strCmdParams[0];

    if (info["MSG"].isNull())
    {
      uint16_t hiddenParamValue;
      cmdSuccess = _Pala.getHiddenParameter(cmdParams[0], &hiddenParamValue);

      if (cmdSuccess)
      {
        String hiddenParamName("HPAR");
        hiddenParamName += cmdParams[0];
        data[hiddenParamName] = hiddenParamValue;
      }
    }
  }

  if (!cmdProcessed && cmd == F("CMD ON"))
  {
    cmdProcessed = true;

    cmdSuccess = _Pala.switchOn();
  }

  if (!cmdProcessed && cmd == F("CMD OFF"))
  {
    cmdProcessed = true;

    cmdSuccess = _Pala.switchOff();
  }

  if (!cmdProcessed && cmd.startsWith(F("SET POWR ")))
  {
    cmdProcessed = true;

    if (cmdParamNumber != 1)
      info["MSG"] = String(F("Incorrect Parameter Number : ")) + cmdParamNumber;
    else if (!validCmdParams[0])
      info["MSG"] = String(F("Incorrect Parameter Value : ")) + strCmdParams[0];

    if (info["MSG"].isNull())
    {
      byte PWRReturn;
      bool isF2LReturnValid;
      uint16_t _F2LReturn;
      uint16_t FANLMINMAXReturn[6];
      cmdSuccess = _Pala.setPower(cmdParams[0], &PWRReturn, &isF2LReturnValid, &_F2LReturn, &FANLMINMAXReturn);

      if (cmdSuccess)
      {
        data["PWR"] = PWRReturn;
        if (isF2LReturnValid)
          data["F2L"] = _F2LReturn;
        JsonArray fanlminmax = data.createNestedArray("FANLMINMAX");
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
      data["PWR"] = PWRReturn;
      if (isF2LReturnValid)
        data["F2L"] = _F2LReturn;
      JsonArray fanlminmax = data.createNestedArray("FANLMINMAX");
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
      data["PWR"] = PWRReturn;
      if (isF2LReturnValid)
        data["F2L"] = _F2LReturn;
      JsonArray fanlminmax = data.createNestedArray("FANLMINMAX");
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

    const __FlashStringHelper *errorMessage[6] = {F("Year"), F("Month"), F("Day"), F("Hour"), F("Minute"), F("Second")};

    if (cmdParamNumber != 6)
      info["MSG"] = String(F("Incorrect Parameter Number : ")) + cmdParamNumber;

    // check cmd parameters validation
    for (byte i = 0; i < 6 && info["MSG"].isNull(); i++)
    {
      if (!validCmdParams[i])
        info["MSG"] = String(F("Incorrect ")) + errorMessage[i] + F(" Value : ") + strCmdParams[i];
    }

    // Check if date is valid
    // basic control
    if (cmdParams[0] < 2000 || cmdParams[0] > 2099)
      info["MSG"] = F("Incorrect Year");
    else if (cmdParams[1] < 1 || cmdParams[1] > 12)
      info["MSG"] = F("Incorrect Month");
    else if ((cmdParams[2] < 1 || cmdParams[2] > 31) ||
             ((cmdParams[2] == 4 || cmdParams[2] == 6 || cmdParams[2] == 9 || cmdParams[2] == 11) && cmdParams[3] > 30) ||                        // 30 days month control
             (cmdParams[2] == 2 && cmdParams[3] > 29) ||                                                                                          // February leap year control
             (cmdParams[2] == 2 && cmdParams[3] == 29 && !(((cmdParams[0] % 4 == 0) && (cmdParams[0] % 100 != 0)) || (cmdParams[0] % 400 == 0)))) // February not leap year control
      info["MSG"] = F("Incorrect Day");
    else if (cmdParams[3] > 23)
      info["MSG"] = F("Incorrect Hour");
    else if (cmdParams[4] > 59)
      info["MSG"] = F("Incorrect Minute");
    else if (cmdParams[5] > 59)
      info["MSG"] = F("Incorrect Second");

    if (info["MSG"].isNull())
    {
      char STOVE_DATETIMEReturn[20];
      byte STOVE_WDAYReturn;
      cmdSuccess = _Pala.setDateTime(cmdParams[0], cmdParams[1], cmdParams[2], cmdParams[3], cmdParams[4], cmdParams[5], &STOVE_DATETIMEReturn, &STOVE_WDAYReturn);

      if (cmdSuccess)
      {
        data["STOVE_DATETIME"] = STOVE_DATETIMEReturn;
        data["STOVE_WDAY"] = STOVE_WDAYReturn;
      }
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET RFAN ")))
  {
    cmdProcessed = true;

    if (cmdParamNumber != 1)
      info["MSG"] = String(F("Incorrect Parameter Number : ")) + cmdParamNumber;
    else if (!validCmdParams[0])
      info["MSG"] = String(F("Incorrect Parameter Value : ")) + strCmdParams[0];

    if (info["MSG"].isNull())
    {
      bool isPWRReturnValid;
      byte PWRReturn;
      uint16_t F2LReturn;
      uint16_t F2LFReturn;
      cmdSuccess = _Pala.setRoomFan(cmdParams[0], &isPWRReturnValid, &PWRReturn, &F2LReturn, &F2LFReturn);

      if (cmdSuccess)
      {
        if (isPWRReturnValid)
          data["PWR"] = PWRReturn;
        data["F2L"] = F2LReturn;
        data["F2LF"] = F2LFReturn;
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
        data["PWR"] = PWRReturn;
      data["F2L"] = F2LReturn;
      data["F2LF"] = F2LFReturn;
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
        data["PWR"] = PWRReturn;
      data["F2L"] = F2LReturn;
      data["F2LF"] = F2LFReturn;
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET FN3L ")))
  {
    cmdProcessed = true;

    if (cmdParamNumber != 1)
      info["MSG"] = String(F("Incorrect Parameter Number : ")) + cmdParamNumber;
    else if (!validCmdParams[0])
      info["MSG"] = String(F("Incorrect Parameter Value : ")) + strCmdParams[0];

    if (info["MSG"].isNull())
    {
      uint16_t F3LReturn;
      cmdSuccess = _Pala.setRoomFan3(cmdParams[0], &F3LReturn);

      if (cmdSuccess)
      {
        data["F3L"] = F3LReturn;
      }
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET FN4L ")))
  {
    cmdProcessed = true;

    if (cmdParamNumber != 1)
      info["MSG"] = String(F("Incorrect Parameter Number : ")) + cmdParamNumber;
    else if (!validCmdParams[0])
      info["MSG"] = String(F("Incorrect Parameter Value : ")) + strCmdParams[0];

    if (info["MSG"].isNull())
    {
      uint16_t F4LReturn;
      cmdSuccess = _Pala.setRoomFan4(cmdParams[0], &F4LReturn);

      if (cmdSuccess)
      {
        data["F4L"] = F4LReturn;
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
      info["CMD"] = F("SET SLNT");
      info["MSG"] = String(F("Incorrect Silent Mode value : ")) + strSilentMode;
    }

    if (info["MSG"].isNull())
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
        data["SLNT"] = SLNTReturn;
        data["PWR"] = PWRReturn;
        data["F2L"] = F2LReturn;
        data["F2LF"] = F2LFReturn;
        if (isF3LF4LReturnValid)
        {
          data["F3L"] = F3LReturn;
          data["F4L"] = F4LReturn;
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
      info["CMD"] = F("SET CSST");
      info["MSG"] = String(F("Incorrect Chrono Status value : ")) + strChronoStatus;
    }

    if (info["MSG"].isNull())
    {
      byte CHRSTATUSReturn;
      cmdSuccess = _Pala.setChronoStatus(chronoStatus, &CHRSTATUSReturn);

      if (cmdSuccess)
      {
        data["CHRSTATUS"] = CHRSTATUSReturn;
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
      info["CMD"] = F("SET CSTH");
      info["MSG"] = String(F("Incorrect Program Number : ")) + strProgramNumber;
    }

    if (info["MSG"].isNull() && startHour == 0 && strStartHour[0] != '0')
    {
      info["CMD"] = F("SET CSTH");
      info["MSG"] = String(F("Incorrect Start Hour : ")) + strStartHour;
    }

    if (info["MSG"].isNull())
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
      info["CMD"] = F("SET CSTM");
      info["MSG"] = String(F("Incorrect Program Number : ")) + strProgramNumber;
    }

    if (info["MSG"].isNull() && startMinute == 0 && strStartMinute[0] != '0')
    {
      info["CMD"] = F("SET CSTM");
      info["MSG"] = String(F("Incorrect Start Minute : ")) + startMinute;
    }

    if (info["MSG"].isNull())
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

      info["CMD"] = F("SET CSPH");
      info["MSG"] = String(F("Incorrect Program Number : ")) + strProgramNumber;
    }

    if (info["MSG"].isNull() && stopHour == 0 && strStopHour[0] != '0')
    {
      info["CMD"] = F("SET CSPH");
      info["MSG"] = String(F("Incorrect Stop Hour : ")) + strStopHour;
    }

    if (info["MSG"].isNull())
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
      info["CMD"] = F("SET CSPM");
      info["MSG"] = String(F("Incorrect Program Number : ")) + strProgramNumber;
    }

    if (info["MSG"].isNull() && stopMinute == 0 && strStopMinute[0] != '0')
    {
      info["CMD"] = F("SET CSPM");
      info["MSG"] = String(F("Incorrect Stop Minute : ")) + strStopMinute;
    }

    if (info["MSG"].isNull())
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
      info["CMD"] = F("SET CSET");
      info["MSG"] = String(F("Incorrect Program Number : ")) + strProgramNumber;
    }

    if (info["MSG"].isNull() && setPoint == 0 && strSetPoint[0] != '0')
    {
      info["CMD"] = F("SET CSET");
      info["MSG"] = String(F("Incorrect SetPoint : ")) + strSetPoint;
    }

    if (info["MSG"].isNull())
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
      info["CMD"] = F("SET CDAY");
      info["MSG"] = String(F("Incorrect Day Number : ")) + strDayNumber;
    }

    if (info["MSG"].isNull() && memoryNumber == 0 && strMemoryNumber[0] != '0')
    {
      info["CMD"] = F("SET CDAY");
      info["MSG"] = String(F("Incorrect Memory Number : ")) + strMemoryNumber;
    }

    if (info["MSG"].isNull() && programNumber == 0 && strProgramNumber[0] != '0')
    {
      info["CMD"] = F("SET CDAY");
      info["MSG"] = String(F("Incorrect Program Number : ")) + strProgramNumber;
    }

    if (info["MSG"].isNull())
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

    for (byte i = 0; i < 6 && info["MSG"].isNull(); i++)
    {
      strParams[i] = cmd.substring(posInCmd, cmd.indexOf(' ', posInCmd));
      params[i] = strParams[i].toInt();
      if (params[i] == 0 && strParams[i][0] != '0')
      {
        info["CMD"] = F("SET CPRD");
        info["MSG"] = String(F("Incorrect ")) + errorMessage[i] + F(" : ") + strParams[i];
      }
      posInCmd += strParams[i].length() + 1;
    }

    if (info["MSG"].isNull())
    {
      cmdSuccess = _Pala.setChronoPrg(params[0], params[1], params[2], params[3], params[4], params[5]);

      if (cmdSuccess)
      {
        char programName[3] = {'P', 'X', 0};
        char time[6] = {'0', '0', ':', '0', '0', 0};

        programName[1] = params[0] + '0';
        JsonObject px = data.createNestedObject(programName);
        px["CHRSETP"] = (float)params[1];
        time[0] = params[2] / 10 + '0';
        time[1] = params[2] % 10 + '0';
        time[3] = params[3] / 10 + '0';
        time[4] = params[3] % 10 + '0';
        px["START"] = time;
        time[0] = params[4] / 10 + '0';
        time[1] = params[4] % 10 + '0';
        time[3] = params[5] / 10 + '0';
        time[4] = params[5] % 10 + '0';
        px["STOP"] = time;
      }
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET SETP ")))
  {
    cmdProcessed = true;

    byte setPoint = cmd.substring(9).toInt();

    if (setPoint == 0)
    {
      info["CMD"] = F("SET SETP");
      info["MSG"] = String(F("Incorrect SetPoint value : ")) + cmd.substring(9);
    }

    if (info["MSG"].isNull())
    {
      float SETPReturn;
      cmdSuccess = _Pala.setSetpoint(setPoint, &SETPReturn);

      if (cmdSuccess)
      {
        data["SETP"] = serialized(String(SETPReturn, 2));
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
      data["SETP"] = serialized(String(SETPReturn, 2));
    }
  }

  if (!cmdProcessed && cmd == F("SET STPD"))
  {
    cmdProcessed = true;

    float SETPReturn;
    cmdSuccess = _Pala.setSetPointDown(&SETPReturn);

    if (cmdSuccess)
    {
      data["SETP"] = serialized(String(SETPReturn, 2));
    }
  }

  if (!cmdProcessed && cmd.startsWith(F("SET STPF ")))
  {
    cmdProcessed = true;

    float setPointFloat = cmd.substring(9).toFloat();

    if (setPointFloat == 0.0f)
    {
      info["CMD"] = F("SET STPF");
      info["MSG"] = String(F("Incorrect SetPoint Float value : ")) + cmd.substring(9);
    }

    if (info["MSG"].isNull())
    {
      float SETPReturn;
      cmdSuccess = _Pala.setSetpoint(setPointFloat, &SETPReturn);

      if (cmdSuccess)
      {
        data["SETP"] = serialized(String(SETPReturn, 2));
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
      info["CMD"] = F("SET PARM");
      info["MSG"] = String(F("Incorrect Parameter Number : ")) + strParamNumber;
    }

    if (info["MSG"].isNull() && paramValue == 0 && strParamValue[0] != '0')
    {
      info["CMD"] = F("SET PARM");
      info["MSG"] = String(F("Incorrect Parameter Value : ")) + strParamValue;
    }

    if (info["MSG"].isNull())
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
      info["CMD"] = F("SET HPAR");
      info["MSG"] = String(F("Incorrect Hidden Parameter Number : ")) + strHiddenParamNumber;
    }

    if (info["MSG"].isNull() && hiddenParamValue == 0 && strHiddenParamValue[0] != '0')
    {
      info["CMD"] = F("SET HPAR");
      info["MSG"] = String(F("Incorrect Hidden Parameter Value : ")) + strHiddenParamValue;
    }

    if (info["MSG"].isNull())
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
    info["CMD"] = cmd;
    // successfully
    if (cmdSuccess)
    {
      info["RSP"] = F("OK");
      jsonDoc["SUCCESS"] = true;

      if (publish)
      {
        String strData;
        serializeJson(data, strData);
        statusEventSourceBroadcast(strData);

        String baseTopic = _ha.mqtt.generic.baseTopic;
        MQTTMan::prepareTopic(baseTopic);

        if (_ha.protocol == HA_PROTO_MQTT && _haSendResult)
        {
          _haSendResult &= publishDataToMqtt(baseTopic, cmd.substring(4), jsonDoc);
        }
      }
    }
    else
    {
      // if there is no MSG in info then stove communication failed
      if (info["MSG"].isNull())
      {
        info["RSP"] = F("TIMEOUT");
        info["MSG"] = F("Stove communication failed");
      }
      else
        info["RSP"] = F("ERROR");

      jsonDoc["SUCCESS"] = false;
      data["NODATA"] = true;
    }
  }
  else
  {
    // command is unknown and not processed
    info["RSP"] = F("ERROR");
    info["CMD"] = F("UNKNOWN");
    info["MSG"] = F("No valid request received");
    jsonDoc["SUCCESS"] = false;
    data["NODATA"] = true;
  }

  // serialize result to the provided strJson
  serializeJson(jsonDoc, strJson);

  return jsonDoc["SUCCESS"].as<bool>();
}

void WebPalaControl::publishTick()
{
  // array of commands to execute
  const char *cmdList[] = {
      "GET STAT",
      "GET TMPS",
      "GET FAND",
      "GET CNTR",
      "GET TIME",
      "GET SETP",
      "GET POWR",
      "GET DPRS"};

  // initialize _haSendResult for publish session
  _haSendResult = true;

  // execute commands
  for (const char *cmd : cmdList)
  {
    String strJson;
    // execute command with publish flag to true
    if (!executePalaCmd(cmd, strJson, true))
      break;
  }
}

void WebPalaControl::udpRequestHandler(WiFiUDP &udpServer)
{

  int packetSize = udpServer.parsePacket();
  if (packetSize <= 0)
    return;

  String strData;
  String strAnswer;

  strData.reserve(packetSize + 1);

  // while udpServer.read() do not return -1, get returned value and add it to strData
  int bufferByte;
  while ((bufferByte = udpServer.read()) >= 0)
    strData += (char)bufferByte;

  // process request
  if (strData.endsWith(F("bridge?")))
    executePalaCmd(F("GET STDT"), strAnswer);
  else if (strData.endsWith(F("bridge?GET ALLS")))
    executePalaCmd(F("GET ALLS"), strAnswer);
  else
    executePalaCmd("", strAnswer);

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
    _mqttMan.setBufferSize(1100); // max JSON size (STDT is the longest one)
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
    LOG_SERIAL.print(F("Stove Serial Number: "));
    LOG_SERIAL.println(SN);
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
  server.on(F("/cgi-bin/sendmsg.lua"), HTTP_GET, [this, &server]()
            {
    String cmd;
    String strJson;

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
        server.keepAlive(false);
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
          server.keepAlive(false);
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
          server.keepAlive(false);
          server.send(200, F("text/json"), toReturn);
          break;
        }

        return;
      }
      else
      {
        server.keepAlive(false);
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
        server.keepAlive(false);
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

          server.keepAlive(false);
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

          server.keepAlive(false);
          server.sendHeader(F("Content-Disposition"), F("attachment; filename=\"HPAR.json\""));
          server.send(200, F("text/json"), toReturn);
          break;
        }

        return;
      }
      else
      {
        server.keepAlive(false);
        server.send(200, F("text/json"), F("{\"INFO\":{\"CMD\":\"BKP HPAR\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}"));
        return;
      }
    }

    // Other commands processed using normal Palazzetti logic
    executePalaCmd(cmd, strJson);

    // send response
    server.keepAlive(false);
    server.send(200, F("text/json"), strJson); });

  // Handle HTTP POST requests (Body contains a JSON)
  server.on(
      F("/cgi-bin/sendmsg.lua"), HTTP_POST, [this, &server]()
      {
        String cmd;
        DynamicJsonDocument jsonDoc(128);
        String strJson;

        DeserializationError error = deserializeJson(jsonDoc, server.arg(F("plain")));

        if (!error && !jsonDoc["command"].isNull())
          cmd = jsonDoc["command"].as<String>();

        // process cmd
        executePalaCmd(cmd, strJson);

        // send response
        server.keepAlive(false);
        server.send(200, F("text/json"), strJson); });
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
