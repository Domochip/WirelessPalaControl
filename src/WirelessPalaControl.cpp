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

      uint16_t F1V, F2V, F1RPM, F2L, F2LF;
      bool isF3SF4SValid;
      float F3S, F4S;
      bool isF3LF4LValid;
      uint16_t F3L, F4L;
      if ((_haSendResult &= _Pala.getFanData(&F1V, &F2V, &F1RPM, &F2L, &F2LF, &isF3SF4SValid, &F3S, &F4S, &isF3LF4LValid, &F3L, &F4L)))
      {
        _haSendResult &= _mqttMan.publish((baseTopic + F("F1V")).c_str(), String(F1V).c_str());
        _statusEventSource.send((String("{\"F1V\":") + F1V + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("F2V")).c_str(), String(F2V).c_str());
        _statusEventSource.send((String("{\"F2V\":") + F2V + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("F2L")).c_str(), String(F2L).c_str());
        _statusEventSource.send((String("{\"F2L\":") + F2L + '}').c_str());
        _haSendResult &= _mqttMan.publish((baseTopic + F("F2LF")).c_str(), String(F2LF).c_str());
        _statusEventSource.send((String("{\"F2LF\":") + F2LF + '}').c_str());
        if (isF3SF4SValid)
        {
          _haSendResult &= _mqttMan.publish((baseTopic + F("F3S")).c_str(), String(F3S).c_str());
          _statusEventSource.send((String("{\"F3S\":") + F3S + '}').c_str());
          _haSendResult &= _mqttMan.publish((baseTopic + F("F4S")).c_str(), String(F4S).c_str());
          _statusEventSource.send((String("{\"F4S\":") + F4S + '}').c_str());
        }
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
      if ((_haSendResult &= _Pala.getDateTime(&STOVE_DATETIME, &STOVE_WDAY)))
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

String WebPalaControl::executePalaCmd(const String &cmd){
  String jsonToReturn;
  bool cmdProcessed = false; //cmd has been processed
  bool cmdSuccess = true; //Palazzetti function calls successful

  //Prepare successful answer
  DynamicJsonDocument jsonDoc(2048);
  JsonObject info = jsonDoc.createNestedObject(F("INFO"));
  info[F("CMD")] = cmd;
  info[F("RSP")] = F("OK");
  jsonDoc[F("SUCCESS")] = true;
  JsonObject data = jsonDoc.createNestedObject(F("DATA"));


  if (!cmdProcessed && cmd == F("GET STDT"))
  {
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
    cmdSuccess &= _Pala.getStaticData(&SN, &SNCHK, &MBTYPE, &MOD, &VER, &CORE, &FWDATE, &FLUID, &SPLMIN, &SPLMAX, &UICONFIG, &HWTYPE, &DSPFWVER, &CONFIG, &PELLETTYPE, &PSENSTYPE, &PSENSLMAX, &PSENSLTSH, &PSENSLMIN, &MAINTPROBE, &STOVETYPE, &FAN2TYPE, &FAN2MODE, &BLEMBMODE, &BLEDSPMODE, &CHRONOTYPE, &AUTONOMYTYPE, &NOMINALPWR);
    
    if (cmdSuccess)
    {
      // ----- WPalaControl generated values -----
      data[F("LABEL")] = WiFi.getHostname();

      // Network infos
      data[F("GWDEVICE")] = F("wlan0"); //always wifi
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
      data[F("WPWR")] = String(WiFi.RSSI()) + F(" dBm"); //need conversion to dBm?
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

      data[F("APLCONN")] = 1; //appliance connected
      data[F("ICONN")] = 0; //internet connected
      
      data[F("CBTYPE")] = F("miniembplug"); //CBox model
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
      data[F("CHRONOTYPE")] = 0; //disable chronothermostat (no planning) (enabled if > 1)
      data[F("AUTONOMYTYPE")] = AUTONOMYTYPE;
      data[F("NOMINALPWR")] = NOMINALPWR;
    }
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd == F("GET LABL"))
  {
    data[F("LABEL")] = WiFi.getHostname();
  }

  if (!cmdProcessed && cmd == F("GET ALLS"))
  {
    bool refreshStatus = false;
    unsigned long currentMillis = millis();
    if ((currentMillis - _lastAllStatusRefreshMillis) > 15000UL) //refresh AllStatus data if it's 15sec old
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
    cmdSuccess &= _Pala.getAllStatus(refreshStatus, &MBTYPE, &MOD, &VER, &CORE, &FWDATE, &APLTS, &APLWDAY, &CHRSTATUS, &STATUS, &LSTATUS, &isMFSTATUSValid, &MFSTATUS, &SETP, &PUMP, &PQT, &F1V, &F1RPM, &F2L, &F2LF, &FANLMINMAX, &F2V, &isF3LF4LValid, &F3L, &F4L, &PWR, &FDR, &DPT, &DP, &IN, &OUT, &T1, &T2, &T3, &T4, &T5, &isSNValid, &SN);

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
      data[F("SETP")] = SETP;
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
      data[F("FDR")] = FDR;
      data[F("DPT")] = DPT;
      data[F("DP")] = DP;
      data[F("IN")] = IN;
      data[F("OUT")] = OUT;
      data[F("T1")] = T1;
      data[F("T2")] = T2;
      data[F("T3")] = T3;
      data[F("T4")] = T4;
      data[F("T5")] = T5;

      data[F("EFLAGS")] = 0; //new ErrorFlags not implemented
      if (isSNValid)
        data[F("SN")] = SN;
    }
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd == F("GET STAT"))
  {
    uint16_t STATUS, LSTATUS;
    cmdSuccess &= _Pala.getStatus(&STATUS, &LSTATUS);

    if (cmdSuccess)
    {
      data[F("STATUS")] = STATUS;
      data[F("LSTATUS")] = LSTATUS;
    }
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd == F("GET TMPS"))
  {
    float T1, T2, T3, T4, T5;
    cmdSuccess &= _Pala.getAllTemps(&T1, &T2, &T3, &T4, &T5);

    if (cmdSuccess)
    {
      data[F("T1")] = T1;
      data[F("T2")] = T2;
      data[F("T3")] = T3;
      data[F("T4")] = T4;
      data[F("T5")] = T5;
    }
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd == F("GET FAND"))
  {
    uint16_t F1V, F2V, F1RPM, F2L, F2LF;
    bool isF3SF4SValid;
    float F3S, F4S;
    bool isF3LF4LValid;
    uint16_t F3L, F4L;
    cmdSuccess &= _Pala.getFanData(&F1V, &F2V, &F1RPM, &F2L, &F2LF, &isF3SF4SValid, &F3S, &F4S, &isF3LF4LValid, &F3L, &F4L);

    if (cmdSuccess)
    {
      data[F("F1V")] = F1V;
      data[F("F2V")] = F2V;
      data[F("F1RPM")] = F1RPM;
      data[F("F2L")] = F2L;
      data[F("F2LF")] = F2LF;
      if (isF3SF4SValid)
      {
        data[F("F3S")] = F3S;
        data[F("F4S")] = F4S;
      }
      if (isF3LF4LValid)
      {
        data[F("F3L")] = F3L;
        data[F("F4L")] = F4L;
      }
    }
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd == F("GET SETP"))
  {
    float SETP;
    cmdSuccess &= _Pala.getSetPoint(&SETP);

    if (cmdSuccess)
    {
      data[F("SETP")] = SETP;
    }
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd == F("GET POWR"))
  {
    byte PWR;
    float FDR;
    cmdSuccess &= _Pala.getPower(&PWR, &FDR);

    if (cmdSuccess)
    {
      data[F("PWR")] = PWR;
      data[F("FDR")] = FDR;
    }
    cmdProcessed = true;
  }

  if (!cmdProcessed && (cmd == F("GET CUNT") || cmd == F("GET CNTR")))
  {
    uint16_t IGN, POWERTIMEh, POWERTIMEm, HEATTIMEh, HEATTIMEm, SERVICETIMEh, SERVICETIMEm, ONTIMEh, ONTIMEm, OVERTMPERRORS, IGNERRORS, PQT;
    cmdSuccess &= _Pala.getCounters(&IGN, &POWERTIMEh, &POWERTIMEm, &HEATTIMEh, &HEATTIMEm, &SERVICETIMEh, &SERVICETIMEm, &ONTIMEh, &ONTIMEm, &OVERTMPERRORS, &IGNERRORS, &PQT);

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
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd == F("GET DPRS"))
  {
    uint16_t DP_TARGET, DP_PRESS;
    cmdSuccess &= _Pala.getDPressData(&DP_TARGET, &DP_PRESS);

    if (cmdSuccess)
    {
      data[F("DP_TARGET")] = DP_TARGET;
      data[F("DP_PRESS")] = DP_PRESS;
    }
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd == F("GET TIME"))
  {
    char STOVE_DATETIME[20];
    byte STOVE_WDAY;
    cmdSuccess &= _Pala.getDateTime(&STOVE_DATETIME, &STOVE_WDAY);

    if (cmdSuccess)
    {
      data[F("STOVE_DATETIME")] = STOVE_DATETIME;
      data[F("STOVE_WDAY")] = STOVE_WDAY;
    }
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd == F("GET IOPT"))
  {
    byte IN_I01, IN_I02, IN_I03, IN_I04;
    byte OUT_O01, OUT_O02, OUT_O03, OUT_O04, OUT_O05, OUT_O06, OUT_O07;
    cmdSuccess &= _Pala.getIO(&IN_I01, &IN_I02, &IN_I03, &IN_I04, &OUT_O01, &OUT_O02, &OUT_O03, &OUT_O04, &OUT_O05, &OUT_O06, &OUT_O07);

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
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd == F("GET SERN"))
  {
    char SN[28];
    cmdSuccess &= _Pala.getSN(&SN);

    if (cmdSuccess)
    {
      data[F("SN")] = SN;
    }
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd.startsWith(F("GET PARM ")))
  {
    String strParamNumber(cmd.substring(9));

    byte paramNumber = strParamNumber.toInt();

    if (paramNumber == 0 && strParamNumber[0] != '0')
    {
      jsonToReturn = F("{\"INFO\":{\"CMD\":\"GET PARM\",\"MSG\":\"Incorrect Parameter Number : ");
      jsonToReturn += strParamNumber;
      jsonToReturn += F("\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}");
      return jsonToReturn;
    }

    byte paramValue;
    cmdSuccess &= _Pala.getParameter(paramNumber, &paramValue);

    if (cmdSuccess)
    {
      data[F("PAR")] = paramValue;
    }
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd.startsWith(F("GET HPAR ")))
  {
    String strHiddenParamNumber(cmd.substring(9));

    byte hiddenParamNumber = strHiddenParamNumber.toInt();

    if (hiddenParamNumber == 0 && strHiddenParamNumber[0] != '0')
    {
      jsonToReturn = F("{\"INFO\":{\"CMD\":\"GET HPAR\",\"MSG\":\"Incorrect Hidden Parameter Number : ");
      jsonToReturn += strHiddenParamNumber;
      jsonToReturn += F("\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}");
      return jsonToReturn;
    }

    uint16_t hiddenParamValue;
    cmdSuccess &= _Pala.getHiddenParameter(hiddenParamNumber, &hiddenParamValue);

    if (cmdSuccess)
    {
      data[F("HPAR")] = hiddenParamValue;
    }
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd.startsWith(F("CMD ")))
  {
    String strOrder(cmd.substring(4));

    if (strOrder != F("ON") && strOrder != F("OFF"))
    {
      jsonToReturn = F("{\"INFO\":{\"CMD\":\"CMD\",\"MSG\":\"Incorrect ON/OFF value : ");
      jsonToReturn += cmd.substring(4);
      jsonToReturn += F("\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}");
      return jsonToReturn;
    }

    if (strOrder == F("ON"))
      cmdSuccess &= _Pala.powerOn();
    else if (strOrder == F("OFF"))
      cmdSuccess &= _Pala.powerOff();

    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd.startsWith(F("SET POWR ")))
  {
    byte powerLevel = cmd.substring(9).toInt();

    if (powerLevel == 0)
    {
      jsonToReturn = F("{\"INFO\":{\"CMD\":\"SET POWR\",\"MSG\":\"Incorrect Power value : ");
      jsonToReturn += cmd.substring(9);
      jsonToReturn += F("\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}");
      return jsonToReturn;
    }

    byte PWRReturn;
    bool isF2LReturnValid;
    uint16_t _F2LReturn;
    uint16_t FANLMINMAXReturn[6];
    cmdSuccess &= _Pala.setPower(powerLevel, &PWRReturn, &isF2LReturnValid, &_F2LReturn, &FANLMINMAXReturn);

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
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd.startsWith(F("SET RFAN ")))
  {
    String strRoomFanLevel(cmd.substring(9));

    byte roomFanLevel = strRoomFanLevel.toInt();

    if (roomFanLevel == 0 && strRoomFanLevel[0] != '0')
    {
      jsonToReturn = F("{\"INFO\":{\"CMD\":\"SET RFAN\",\"MSG\":\"Incorrect Room Fan value : ");
      jsonToReturn += strRoomFanLevel;
      jsonToReturn += F("\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}");
      return jsonToReturn;
    }

    bool isPWRReturnValid;
    byte PWRReturn;
    uint16_t F2LReturn;
    uint16_t F2LFReturn;
    cmdSuccess &= _Pala.setRoomFan(roomFanLevel, &isPWRReturnValid, &PWRReturn, &F2LReturn, &F2LFReturn);

    if (cmdSuccess)
    {
      if (isPWRReturnValid)
        data[F("PWR")] = PWRReturn;
      data[F("F2L")] = F2LReturn;
      data[F("F2LF")] = F2LFReturn;
    }
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd.startsWith(F("SET FN3L ")))
  {
    String strRoomFan3Level(cmd.substring(9));

    byte roomFan3Level = strRoomFan3Level.toInt();

    if (roomFan3Level == 0 && strRoomFan3Level[0] != '0')
    {
      jsonToReturn = F("{\"INFO\":{\"CMD\":\"SET FN3L\",\"MSG\":\"Incorrect Room Fan 3 value : ");
      jsonToReturn += strRoomFan3Level;
      jsonToReturn += F("\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}");
      return jsonToReturn;
    }

    uint16_t F3LReturn;
    cmdSuccess &= _Pala.setRoomFan3(roomFan3Level, &F3LReturn);

    if (cmdSuccess)
    {
      data[F("F3L")] = F3LReturn;
    }
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd.startsWith(F("SET FN4L ")))
  {
    String strRoomFan4Level(cmd.substring(9));

    byte roomFan4Level = strRoomFan4Level.toInt();

    if (roomFan4Level == 0 && strRoomFan4Level[0] != '0')
    {
      jsonToReturn = F("{\"INFO\":{\"CMD\":\"SET FN4L\",\"MSG\":\"Incorrect Room Fan 4 value : ");
      jsonToReturn += strRoomFan4Level;
      jsonToReturn += F("\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}");
      return jsonToReturn;
    }

    uint16_t F4LReturn;
    cmdSuccess &= _Pala.setRoomFan4(roomFan4Level, &F4LReturn);

    if (cmdSuccess)
    {
      data[F("F4L")] = F4LReturn;
    }
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd.startsWith(F("SET SLNT ")))
  {
    String strSilentMode = cmd.substring(9);

    byte silentMode = strSilentMode.toInt();

    if (silentMode == 0 && strSilentMode[0] != '0')
    {
      jsonToReturn = F("{\"INFO\":{\"CMD\":\"SET SLNT\",\"MSG\":\"Incorrect Silent Mode value : ");
      jsonToReturn += strSilentMode;
      jsonToReturn += F("\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}");
      return jsonToReturn;
    }

    byte SLNTReturn;
    byte PWRReturn;
    uint16_t F2LReturn;
    uint16_t F2LFReturn;
    bool isF3LF4LReturnValid;
    uint16_t F3LReturn;
    uint16_t F4LReturn;
    cmdSuccess &= _Pala.setSilentMode(silentMode, &SLNTReturn, &PWRReturn, &F2LReturn, &F2LFReturn, &isF3LF4LReturnValid, &F3LReturn, &F4LReturn);

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
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd.startsWith(F("SET SETP ")))
  {
    byte setPoint = cmd.substring(9).toInt();

    if (setPoint == 0)
    {
      jsonToReturn = F("{\"INFO\":{\"CMD\":\"SET SETP\",\"MSG\":\"Incorrect SetPoint value : ");
      jsonToReturn += cmd.substring(9);
      jsonToReturn += F("\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}");
      return jsonToReturn;
    }

    float SETPReturn;
    cmdSuccess &= _Pala.setSetpoint(setPoint, &SETPReturn);

    if (cmdSuccess)
    {
      data[F("SETP")] = SETPReturn;
    }
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd.startsWith(F("SET STPF ")))
  {
    float setPointFloat = cmd.substring(9).toFloat();

    if (setPointFloat == 0.0f)
    {
      jsonToReturn = F("{\"INFO\":{\"CMD\":\"SET STPF\",\"MSG\":\"Incorrect SetPoint Float value : ");
      jsonToReturn += cmd.substring(9);
      jsonToReturn += F("\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}");
      return jsonToReturn;
    }

    float SETPReturn;
    cmdSuccess &= _Pala.setSetpoint(setPointFloat, &SETPReturn);

    if (cmdSuccess)
    {
      data[F("SETP")] = SETPReturn;
    }
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd.startsWith(F("SET PARM ")))
  {
    String strParamNumber(cmd.substring(9, cmd.indexOf(' ', 9)));
    String strParamValue(cmd.substring(cmd.indexOf(' ', 9) + 1));

    byte paramNumber = strParamNumber.toInt();

    if (paramNumber == 0 && strParamNumber[0] != '0')
    {
      jsonToReturn = F("{\"INFO\":{\"CMD\":\"SET PARM\",\"MSG\":\"Incorrect Parameter Number : ");
      jsonToReturn += strParamNumber;
      jsonToReturn += F("\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}");
      return jsonToReturn;
    }

    byte paramValue = strParamValue.toInt();

    if (paramValue == 0 && strParamValue[0] != '0')
    {
      jsonToReturn = F("{\"INFO\":{\"CMD\":\"SET PARM\",\"MSG\":\"Incorrect Parameter Value : ");
      jsonToReturn += strParamValue;
      jsonToReturn += F("\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}");
      return jsonToReturn;
    }

    cmdSuccess &= _Pala.setParameter(paramNumber, paramValue);

    if (cmdSuccess)
    {
      data[String(F("PAR")) + paramNumber] = paramValue;
    }
    cmdProcessed = true;
  }

  if (!cmdProcessed && cmd.startsWith(F("SET HPAR ")))
  {
    String strHiddenParamNumber(cmd.substring(9, cmd.indexOf(' ', 9)));
    String strHiddenParamValue(cmd.substring(cmd.indexOf(' ', 9) + 1));

    byte hiddenParamNumber = strHiddenParamNumber.toInt();

    if (hiddenParamNumber == 0 && strHiddenParamNumber[0] != '0')
    {
      jsonToReturn = F("{\"INFO\":{\"CMD\":\"SET HPAR\",\"MSG\":\"Incorrect Hidden Parameter Number : ");
      jsonToReturn += strHiddenParamNumber;
      jsonToReturn += F("\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}");
      return jsonToReturn;
    }

    uint16_t hiddenParamValue = strHiddenParamValue.toInt();

    if (hiddenParamValue == 0 && strHiddenParamValue[0] != '0')
    {
      jsonToReturn = F("{\"INFO\":{\"CMD\":\"SET HPAR\",\"MSG\":\"Incorrect Hidden Parameter Value : ");
      jsonToReturn += strHiddenParamValue;
      jsonToReturn += F("\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}");
      return jsonToReturn;
    }

    cmdSuccess &= _Pala.setHiddenParameter(hiddenParamNumber, hiddenParamValue);
    
    if (cmdSuccess)
    {
      data[String(F("HPAR")) + hiddenParamNumber] = hiddenParamValue;
    }
    cmdProcessed = true;
  }


  // if command has been processed
  if (cmdProcessed)
  {
    //successfully
    if (cmdSuccess) serializeJson(jsonDoc, jsonToReturn); //serialize jsonToReturn
    else
    {
      //stove communication failed
      jsonToReturn = F("{\"INFO\":{\"CMD\":\"");
      jsonToReturn += cmd;
      jsonToReturn += F("\",\"MSG\":\"Stove communication failed\",\"RSP\":\"TIMEOUT\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}");
    }
  }
  else{
    // command is unknown and not processed
    jsonToReturn = F("{\"INFO\":{\"CMD\":\"UNKNOWN\",\"MSG\":\"No valid request received\"},\"SUCCESS\":false,\"DATA\":{\"NODATA\":true}}");
  }

  return jsonToReturn;
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
  //Stop UdpServer
  _udpServer.close();

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

  //if no HA, then use default period for Publish
  if (_ha.protocol != HA_PROTO_DISABLED)
  {
    if (res)
      publishTick(); //if configuration changed, publish immediately
    _publishTicker.attach(_ha.uploadPeriod, [this]() { this->_needPublish = true; });
  }

  //Start UDP Server
  _udpServer.listen(54549);
  _udpServer.onPacket([this](AsyncUDPPacket packet){
    String strData;
    strData.reserve(packet.length()+1);
    String strAnswer;

    uint8_t* data = packet.data();

    // read request received through UDP
    for (size_t i = 0; i < packet.length(); i++) strData += (char)data[i];

    // process request
    if (strData.endsWith(F("bridge?"))) strAnswer = executePalaCmd(F("GET STDT"));
    else if (strData.endsWith(F("bridge?GET ALLS"))) strAnswer = executePalaCmd(F("GET ALLS"));
    else strAnswer = executePalaCmd("");

    // answer to the requester
    packet.write((const uint8_t *)strAnswer.c_str(), strAnswer.length());
  });

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
  // Handle HTTP GET requests
  server.on("/cgi-bin/sendmsg.lua", HTTP_GET, [this](AsyncWebServerRequest *request) {
    String cmd;

    if (request->hasParam(F("cmd"))) cmd = request->getParam(F("cmd"))->value();

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
        request->send(200, F("text/json"), ret);
        return;
      }

      byte params[0x6A];
      res &= _Pala.getAllParameters(&params);

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
        request->send(200, F("text/json"), ret);
        return;
      }

      uint16_t hiddenParams[0x6F];
      res &= _Pala.getAllHiddenParameters(&hiddenParams);

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

    // Other commands processed using normal Palazzetti logic
    request->send(200, F("text/json"), executePalaCmd(cmd));
  });

  // Commented best method because of issue : https://github.com/me-no-dev/ESPAsyncWebServer/pull/1105
  // // Handle HTTP POST requests (Body contains a JSON)
  // server.addHandler(new AsyncCallbackJsonWebHandler("/cgi-bin/sendmsg.lua", [this](AsyncWebServerRequest *request, JsonVariant &json) {
  //   String cmd;
    
  //   if (!json[F("command")].isNull()) {
  //     cmd = json[F("command")].as<String>();
  //   }
    
  //   // process cmd
  //   request->send(200, F("text/json"), executePalaCmd(cmd));
  //   return;
  // }, 128)); // last param is DynamicJsonDocument size

  // Handle HTTP POST requests (Body contains a JSON)
  server.on("/cgi-bin/sendmsg.lua", HTTP_POST,[this](AsyncWebServerRequest *request) {
    DynamicJsonDocument jsonBuffer(128);
    String cmd;

    DeserializationError error = deserializeJson(jsonBuffer, (char*)(request->_tempObject));

    if (!error && !jsonBuffer[F("command")].isNull()) cmd = jsonBuffer[F("command")].as<String>();

    // process cmd
    request->send(200, F("text/json"), executePalaCmd(cmd));

  }, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if (total > 0 && request->_tempObject == NULL) {
      request->_tempObject = malloc(total+1);
      if (request->_tempObject != NULL) ((char*)(request->_tempObject))[total] = 0;
    }
    if (request->_tempObject != NULL) {
      memcpy((uint8_t*)(request->_tempObject) + index, data, len);
    }
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
