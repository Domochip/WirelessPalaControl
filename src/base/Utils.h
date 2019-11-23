#ifndef Utils_h
#define Utils_h

#include <arduino.h>

class Utils {
  public:
    static byte asciiToHex(char c); //Utils
    static bool isFingerPrintEmpty(byte* fingerPrintArray);
    static bool fingerPrintS2A(byte* fingerPrintArray, const char* fingerPrintToDecode);
    static char* fingerPrintA2S(char* fpBuffer, byte* fingerPrintArray, char separator = 0);
};

#endif