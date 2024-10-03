#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
using WebServer = ESP8266WebServer;
#else
#include <WiFi.h>
#include <WebServer.h>
#endif
#include <EEPROM.h>
#include <LittleFS.h>
#include <FS.h>

#include "Version.h"
#include "../Main.h"
#include "Application.h"
#include "Core.h"
#include "WifiMan.h"

// include Application header file
#include APPLICATION1_HEADER

// System
Core core('0', "Core");

// WifiMan
WifiMan wifiMan('w', "WiFi");

// WebServer
WebServer server(80);
// flag to pause application Run during Firmware Update
bool pauseApplication = false;
// variable used by objects to indicate system reboot is required
bool shouldReboot = false;

// Application1 object
APPLICATION1_CLASS application1('1', APPLICATION1_NAME);

//-----------------------------------------------------------------------
// Setup function
//-----------------------------------------------------------------------
void setup()
{
#ifdef LOG_SERIAL
  LOG_SERIAL.begin(LOG_SERIAL_SPEED);
  LOG_SERIAL.println();
  LOG_SERIAL.println();
  LOG_SERIAL.flush();
#endif

#ifdef STATUS_LED_SETUP
  STATUS_LED_SETUP
#endif
#ifdef STATUS_LED_ERROR
  STATUS_LED_ERROR
#endif

#ifdef LOG_SERIAL
  LOG_SERIAL.print(F(APPLICATION1_DESC " "));
  LOG_SERIAL.println(VERSION);
  LOG_SERIAL.println(F("---Booting---"));
#endif

#ifndef RESCUE_BUTTON_WAIT
#define RESCUE_BUTTON_WAIT 3
#endif

  bool skipExistingConfig = false;

  // look into EEPROM for Rescue mode flag
  EEPROM.begin(4);
  skipExistingConfig = EEPROM.read(0) != 0;
  if (skipExistingConfig)
    EEPROM.write(0, 0);
  EEPROM.end();

#ifdef RESCUE_BTN_PIN
  // if config already skipped, don't wait for rescue button
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

#ifdef LOG_SERIAL
  if (skipExistingConfig)
  {
    LOG_SERIAL.println(F("-> RESCUE MODE : Stored configuration won't be loaded."));
  }
#endif
#ifdef ESP8266
  if (!LittleFS.begin())
#else
  if (!LittleFS.begin(true))
#endif
  {
#ifdef LOG_SERIAL
    LOG_SERIAL.println(F("/!\\   File System Mount Failed   /!\\"));
    LOG_SERIAL.println(F("/!\\ Configuration can't be saved /!\\"));
#endif
  }

  // Init Core
  core.init(skipExistingConfig);

  // Init WiFi
  wifiMan.init(skipExistingConfig);

  // Init Application
  application1.init(skipExistingConfig);

#ifdef LOG_SERIAL
  LOG_SERIAL.print(F("Start WebServer : "));
#endif
  core.initWebServer(server, shouldReboot, pauseApplication);
  wifiMan.initWebServer(server, shouldReboot, pauseApplication);
  application1.initWebServer(server, shouldReboot, pauseApplication);

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

  // Handle WebServer
  server.handleClient();

  if (!pauseApplication)
  {
    application1.run();
  }

  wifiMan.run();

  if (shouldReboot)
  {
#ifdef LOG_SERIAL
    LOG_SERIAL.println("Rebooting...");
    delay(100);
    LOG_SERIAL.end();
#endif
    ESP.restart();
  }
}
