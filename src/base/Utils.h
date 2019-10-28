#ifndef Utils_h
#define Utils_h

#include <arduino.h>

class Utils {
  public:
    static byte AsciiToHex(char c); //Utils
    static bool IsFingerPrintEmpty(byte* fingerPrintArray);
    static bool FingerPrintS2A(byte* fingerPrintArray, const char* fingerPrintToDecode);
    static char* FingerPrintA2S(char* fpBuffer, byte* fingerPrintArray, char separator = 0);
};

#endif