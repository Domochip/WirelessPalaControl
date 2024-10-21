#ifndef Main_h
#define Main_h

#include <Arduino.h>

// Helper macros to convert a define to a string
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// ----- Should be modified for your application -----
#define APPLICATION1_MANUFACTURER "Domochip"
#define APPLICATION1_MODEL "WPalaControl"
#define APPLICATION1_CLASS WPalaControl
#define VERSION_NUMBER "3.1.11"

#define APPLICATION1_NAME TOSTRING(APPLICATION1_CLASS)     // stringified class name
#define APPLICATION1_HEADER TOSTRING(APPLICATION1_CLASS.h) // calculated header file "{APPLICATION1_NAME}.h"
#define DEFAULT_AP_SSID APPLICATION1_NAME                  // Default Access Point SSID "{APPLICATION1_NAME}{4 digits of ChipID}"
#define DEFAULT_AP_PSK APPLICATION1_NAME "Pass"            // Default Access Point Password "{APPLICATION1_NAME}Pass"

// Control EventSourceMan code (To be used by Application if EventSource server is needed)
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
