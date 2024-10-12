#ifndef Main_h
#define Main_h

#include <Arduino.h>

// DomoChip Informations
// Configuration Web Pages :
// http://IP/

#define APPLICATION1_HEADER "WirelessPalaControl.h"
#define APPLICATION1_NAME "WPalaControl"
#define APPLICATION1_DESC "DomoChip Wireless Palazzetti Control"
#define APPLICATION1_CLASS WebPalaControl

#define VERSION_NUMBER "3.1.8"

#define DEFAULT_AP_SSID "WirelessPalaControl"
#define DEFAULT_AP_PSK "PasswordPalaControl"

// Enable status webpage EventSource
#define EVTSRC_ENABLED 1
#define EVTSRC_MAX_CLIENTS 2
#define EVTSRC_KEEPALIVE_ENABLED 0

// Enable developper mode (SPIFFS editor)
#define DEVELOPPER_MODE 0

// Log Serial Object
#ifdef ESP8266
#define LOG_SERIAL Serial1
#else
#define LOG_SERIAL Serial
#endif
// Choose Log Serial Speed
#define LOG_SERIAL_SPEED 38400

// Choose Pin used to boot in Rescue Mode
// #define RESCUE_BTN_PIN 2

// Define time to wait for Rescue press (in s)
// #define RESCUE_BUTTON_WAIT 3

// Status LED
// #define STATUS_LED_SETUP pinMode(XX, OUTPUT);pinMode(XX, OUTPUT);
// #define STATUS_LED_OFF digitalWrite(XX, HIGH);digitalWrite(XX, HIGH);
// #define STATUS_LED_ERROR digitalWrite(XX, HIGH);digitalWrite(XX, HIGH);
// #define STATUS_LED_WARNING digitalWrite(XX, HIGH);digitalWrite(XX, HIGH);
// #define STATUS_LED_GOOD digitalWrite(XX, HIGH);digitalWrite(XX, HIGH);

// construct Version text
#if DEVELOPPER_MODE
#define VERSION VERSION_NUMBER "-DEV"
#else
#define VERSION VERSION_NUMBER
#endif

#endif
