#include "Utils.h"

//------------------------------------------
//simple function that convert an hexadecimal char to byte
byte Utils::asciiToHex(char c)
{
  return (c < 0x3A) ? (c - 0x30) : (c > 0x60 ? c - 0x57 : c - 0x37);
}

//------------------------------------------
// Function that return true if all byte of fingerprint are empty
bool Utils::isFingerPrintEmpty(byte *fingerPrintArray)
{
  bool ret = true;
  for (byte i = 0; i < 20; i++)
    ret &= (fingerPrintArray[i] == 0);
  return ret;
};

//------------------------------------------
// Function to decode https FingerPrint String into array of 20 bytes
bool Utils::fingerPrintS2A(byte *fingerPrintArray, const char *fingerPrintToDecode)
{

  if (strlen(fingerPrintToDecode) < 40)
    return false;

  //first set array bytes to 0;
  memset(fingerPrintArray, 0, 20);

  byte arrayPos = 0;
  for (byte i = 0; i < strlen(fingerPrintToDecode); i++)
  {

    if (fingerPrintToDecode[i] != ' ' && fingerPrintToDecode[i] != ':' && fingerPrintToDecode[i] != '-')
    {
      fingerPrintArray[arrayPos / 2] += asciiToHex(fingerPrintToDecode[i]);
      if (arrayPos % 2 == 0)
        fingerPrintArray[arrayPos / 2] *= 0x10;
      arrayPos++;
    }
    if (arrayPos == 40)
      return false;
  }

  return true;
}
//------------------------------------------
// Function that convert fingerprint Array to char array (with separator) (char array need to be provided)
char *Utils::fingerPrintA2S(char *fpBuffer, byte *fingerPrintArray, char separator)
{

  fpBuffer[0] = 0;

  for (byte i = 0; i < 20; i++)
  {
    sprintf_P(fpBuffer, PSTR("%s%02x"), fpBuffer, fingerPrintArray[i]);
    if (i != 19 && separator != 0)
    {
      fpBuffer[strlen(fpBuffer) + 1] = 0;
      fpBuffer[strlen(fpBuffer)] = separator;
    }
  }
  return fpBuffer;
}
