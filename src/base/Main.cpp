#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <FS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "Version.h"
#include "..\Main.h"
#include "Application.h"
#include "Core.h"
#include "WifiMan.h"

//include Application header files
#ifdef APPLICATION1_HEADER
#include APPLICATION1_HEADER
#endif
#ifdef APPLICATION2_HEADER
#include APPLICATION2_HEADER
#endif
#ifdef APPLICATION3_HEADER
#include APPLICATION3_HEADER
#endif

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
#ifdef LOG_SERIAL
  LOG_SERIAL.begin(LOG_SERIAL_SPEED);
  LOG_SERIAL.println();
  LOG_SERIAL.println();
  delay(200);
#endif

#ifdef STATUS_LED_SETUP
  STATUS_LED_SETUP
#endif
#ifdef STATUS_LED_ERROR
  STATUS_LED_ERROR
#endif

#ifdef LOG_SERIAL
  LOG_SERIAL.print(F(APPLICATION1_DESC " "));
  LOG_SERIAL.println(BASE_VERSION "/" VERSION);
  LOG_SERIAL.println(F("---Booting---"));
#endif

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
#ifdef LOG_SERIAL
    LOG_SERIAL.print(F("Wait Rescue button for "));
    LOG_SERIAL.print(RESCUE_BUTTON_WAIT);
    LOG_SERIAL.println(F(" seconds"));
#endif

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
  {
#ifdef LOG_SERIAL
    LOG_SERIAL.println(F("-> RESCUE MODE : Stored configuration won't be loaded."));
#endif
  }
  if (!SPIFFS.begin())
  {
#ifdef LOG_SERIAL
    LOG_SERIAL.println(F("/!\\   File System Mount Failed   /!\\"));
    LOG_SERIAL.println(F("/!\\ Configuration can't be saved /!\\"));
#endif
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

#ifdef LOG_SERIAL
  LOG_SERIAL.print(F("Start WebServer : "));
#endif
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
#ifdef LOG_SERIAL
  LOG_SERIAL.println(F("OK"));

  LOG_SERIAL.println(F("---End of setup()---"));
#endif
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
#ifdef LOG_SERIAL
    LOG_SERIAL.println("Rebooting...");
    delay(100);
    LOG_SERIAL.end();
#endif
    ESP.restart();
  }
  yield();
}
