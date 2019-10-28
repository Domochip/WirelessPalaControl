#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <FS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "Version.h"
#include "Application.h"
#include "Core.h"
#include "WifiMan.h"
#include "..\Main.h"

//System
Core core('0', "Core");

//WifiMan
WifiMan wifiMan('w', "WiFi");

//AsyncWebServer
AsyncWebServer server(80);
//flag to pause application Run during Firmware Update
bool pauseApplication = false;
//variable used by objects to indicate system reboot is required
bool shouldReboot = false;

//Application1 object
#ifdef APPLICATION1_CLASS
APPLICATION1_CLASS application1('1', APPLICATION1_NAME);
#endif
//Application2 object
#ifdef APPLICATION2_CLASS
APPLICATION2_CLASS application2('2', APPLICATION2_NAME);
#endif
//Application3 object
#ifdef APPLICATION3_CLASS
APPLICATION3_CLASS application3('3', APPLICATION3_NAME);
#endif

//-----------------------------------------------------------------------
// Setup function
//-----------------------------------------------------------------------
void setup()
{

  Serial.begin(SERIAL_SPEED);
  Serial.println();
  Serial.println();
  delay(200);

#ifdef STATUS_LED_SETUP
  STATUS_LED_SETUP
#endif
#ifdef STATUS_LED_ERROR
  STATUS_LED_ERROR
#endif

  Serial.print(F(APPLICATION1_DESC " "));
  Serial.println(BASE_VERSION "/" VERSION);
  Serial.println(F("---Booting---"));

#ifndef RESCUE_BUTTON_WAIT
#define RESCUE_BUTTON_WAIT 3
#endif

  bool skipExistingConfig = false;

  //look into EEPROM for Rescue mode flag
  EEPROM.begin(4);
  skipExistingConfig = EEPROM.read(0) != 0;
  if (skipExistingConfig)
    EEPROM.write(0, 0);
  EEPROM.end();

#ifdef RESCUE_BTN_PIN
  //if config already skipped, don't wait for rescue button
  if (!skipExistingConfig)
  {
    Serial.print(F("Wait Rescue button for "));
    Serial.print(RESCUE_BUTTON_WAIT);
    Serial.println(F(" seconds"));

    pinMode(RESCUE_BTN_PIN, (RESCUE_BTN_PIN != 16) ? INPUT_PULLUP : INPUT);
    for (int i = 0; i < 100 && skipExistingConfig == false; i++)
    {
      if (digitalRead(RESCUE_BTN_PIN) == LOW)
        skipExistingConfig = true;
      delay(RESCUE_BUTTON_WAIT * 10);
    }
  }
#endif

  if (skipExistingConfig)
    Serial.println(F("-> RESCUE MODE : Stored configuration won't be loaded."));

  if (!SPIFFS.begin())
  {
    Serial.println(F("/!\\   File System Mount Failed   /!\\"));
    Serial.println(F("/!\\ Configuration can't be saved /!\\"));
  }

  //Init Core
  core.Init(skipExistingConfig);

  //Init WiFi
  wifiMan.Init(skipExistingConfig);

//Init Application
#ifdef APPLICATION1_CLASS
  application1.Init(skipExistingConfig);
#endif
#ifdef APPLICATION2_CLASS
  application2.Init(skipExistingConfig);
#endif
#ifdef APPLICATION3_CLASS
  application3.Init(skipExistingConfig);
#endif

  Serial.print(F("Start WebServer : "));

  core.InitWebServer(server, shouldReboot, pauseApplication);
  wifiMan.InitWebServer(server, shouldReboot, pauseApplication);
#ifdef APPLICATION1_CLASS
  application1.InitWebServer(server, shouldReboot, pauseApplication);
#endif
#ifdef APPLICATION2_CLASS
  application2.InitWebServer(server, shouldReboot, pauseApplication);
#endif
#ifdef APPLICATION3_CLASS
  application3.InitWebServer(server, shouldReboot, pauseApplication);
#endif
  server.begin();
  Serial.println(F("OK"));

  Serial.println(F("---End of setup()---"));
}

//-----------------------------------------------------------------------
// Main Loop function
//-----------------------------------------------------------------------
void loop(void)
{

  if (!pauseApplication)
  {
#ifdef APPLICATION1_CLASS
    application1.Run();
#endif
#ifdef APPLICATION2_CLASS
    application2.Run();
#endif
#ifdef APPLICATION3_CLASS
    application3.Run();
#endif
  }

  wifiMan.Run();

  if (shouldReboot)
  {
    Serial.println("Rebooting...");
    delay(100);
    Serial.end();
    ESP.restart();
  }
  yield();
}
