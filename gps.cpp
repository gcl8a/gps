#include <gps.h>

uint8_t GPSdatum::ParseNMEA(const String& nmeaStr)
{
  Serial.println(nmeaStr);
  
  uint16_t length = nmeaStr.length();
  if(length < 3) return 0;
  
  if(nmeaStr[0] != '$') return 0;
  if(nmeaStr[length - 3] != '*') return 0;
  
  uint8_t checksum = CalcChecksum(nmeaStr.substring(1, length - 3));
  String checksumStr = String(checksum, HEX);
  checksumStr.toUpperCase();
  String checksumNMEA = nmeaStr.substring(length - 2);

  if(checksumStr != checksumNMEA) return 0;

  String NMEAtype = GetNMEASubstring(nmeaStr, 0);

  if (NMEAtype == "GPGGA")
  {
    gpsFix = GetNMEASubstring(nmeaStr, 6).toInt();
    if(!gpsFix) return 0;

    // time
    NMEAtoTime(GetNMEASubstring(nmeaStr, 1));   
    
    lat = ConvertToDMM(GetNMEASubstring(nmeaStr, 2));
    if(GetNMEASubstring(nmeaStr, 3) == String('S')) lat *= -1;

    lon = ConvertToDMM(GetNMEASubstring(nmeaStr, 4));
    if(GetNMEASubstring(nmeaStr, 5) == String('W')) lon *= -1;

    elev = GetNMEASubstring(nmeaStr, 9).toFloat();

    source = GGA;
    return GGA;
  }

  if(NMEAtype == "GPRMC")
  {
    if(GetNMEASubstring(nmeaStr, 2) != String('A')) return 0;
    else gpsFix = 1;
    
    lat = ConvertToDMM(GetNMEASubstring(nmeaStr, 3));
    if(GetNMEASubstring(nmeaStr, 4) == String('S')) lat *= -1;

    lon = ConvertToDMM(GetNMEASubstring(nmeaStr, 5));
    if(GetNMEASubstring(nmeaStr, 6) == String('W')) lon *= -1;

    speed = GetNMEASubstring(nmeaStr, 7).toFloat();

    NMEAtoTime(GetNMEASubstring(nmeaStr, 1));
    NMEAtoDate(GetNMEASubstring(nmeaStr, 9));

    source = RMC;
    return RMC;
  }

  return 0;
}

String GPSdatum::GetNMEASubstring(const String& str, int commaIndex)
/*
 * grabs the substring of an NMEA string after comma number commaIndex
 */
{
  int iComma = 0;
  for(int i = 0; i < commaIndex; i++)
  {
    iComma = str.indexOf(',', iComma + 1);
    if(iComma == -1) return String("");
  }

  int iCommaNext = str.indexOf(',', iComma + 1);
  if(iCommaNext == -1) return String("");

  return str.substring(iComma + 1, iCommaNext);
}

long GPSdatum::ConvertToDMM(const String& degStr)
{
  int iDecimal = degStr.indexOf('.');
  if(iDecimal == -1) return 0;

  long dmm = degStr.substring(0, iDecimal - 2).toInt() * 600000L;
  dmm += degStr.substring(iDecimal - 2, iDecimal).toInt() * 10000L;
  dmm += degStr.substring(iDecimal + 1).toInt();

  return dmm;
}

int GPSdatum::NMEAtoTime(const String& timeStr)
{
  if(timeStr.length() != 10) return 0;

  hour = timeStr.substring(0, 2).toInt();
  minute = timeStr.substring(2, 4).toInt();
  second = timeStr.substring(4, 6).toInt();

  return 1;
}

int GPSdatum::NMEAtoDate(const String& dateStr)
{
  if(dateStr.length() != 6) return 0;

  day = dateStr.substring(0, 2).toInt();
  month = dateStr.substring(2, 4).toInt();
  year = dateStr.substring(4, 6).toInt();
  
  return 1;
}

String GPS::MakeNMEAwithChecksum(const String& str)
{
  String checkSum = String(GPSdatum::CalcChecksum(str), HEX);
  checkSum.toUpperCase();
  return '$' + str + '*' + checkSum + "\r\n";
}
