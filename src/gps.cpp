#include <gps.h>

String GPSDatum::GetNMEASubstring(const String& str, int commaIndex)
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

long GPSDatum::ConvertToDMM(const String& degStr) // "decimilliminutes": divide be 10000 to get minutes
{
  int iDecimal = degStr.indexOf('.');
  if(iDecimal == -1) return 0;

  long dmm = degStr.substring(0, iDecimal - 2).toInt() * 600000L;
  dmm += degStr.substring(iDecimal - 2, iDecimal).toInt() * 10000L;
  dmm += degStr.substring(iDecimal + 1).toInt();

  return dmm;
}

int GPSDatum::NMEAtoTime(const String& timeStr)
{
  if(timeStr.length() < 6) return 0;

  hour = timeStr.substring(0, 2).toInt();
  minute = timeStr.substring(2, 4).toInt();
  second = timeStr.substring(4, 6).toInt();
    
    int8_t decIndex = timeStr.indexOf('.');
    if(decIndex != -1)
        msec = timeStr.substring(decIndex + 1).toInt();
    else
        msec = 0;
    
  return 1;
}

int GPSDatum::NMEAtoDate(const String& dateStr)
{
  if(dateStr.length() != 6) return 0;

  day = dateStr.substring(0, 2).toInt();
  month = dateStr.substring(2, 4).toInt();
  year = dateStr.substring(4, 6).toInt();
  
  return 1;
}

String GPS::MakeNMEAwithChecksum(const String& str)
{
  String checkSum = String(CalcChecksum(str), HEX);
  checkSum.toUpperCase();
  return '$' + str + '*' + checkSum + "\r\n";
}

GPSDatum GPS::ParseNMEA(const String& nmeaStr)
{
    //SerialUSB.println(nmeaStr);
    GPSDatum gpsDatum;
    
    uint16_t length = nmeaStr.length();
    if(length < 3) return 0;
    
    if(nmeaStr[0] != '$') return 0;
    if(nmeaStr[length - 3] != '*') return 0;
    
    uint8_t checksum = CalcChecksum(nmeaStr.substring(1, length - 3));
    String checksumStr = String(checksum, HEX);
    checksumStr.toUpperCase();
    String checksumNMEA = nmeaStr.substring(length - 2);
    
    if(checksumStr != checksumNMEA) return gpsDatum;
    
    String NMEAtype = GetNMEASubstring(nmeaStr, 0);
    
    if (NMEAtype == "GPGGA")
    {
        gpsDatum.gpsFix = GetNMEASubstring(nmeaStr, 6).toInt();
        if(!gpsDatum.gpsFix) return gpsDatum;
        
        // time
        gpsDatum.NMEAtoTime(GetNMEASubstring(nmeaStr, 1));
        
        gpsDatum.lat = ConvertToDMM(GetNMEASubstring(nmeaStr, 2));
        if(GetNMEASubstring(nmeaStr, 3) == String('S')) gpsDatum.lat *= -1;
        
        gpsDatum.lon = ConvertToDMM(GetNMEASubstring(nmeaStr, 4));
        if(GetNMEASubstring(nmeaStr, 5) == String('W')) gpsDatum.lon *= -1;
        
        gpsDatum.elevDM = GetNMEASubstring(nmeaStr, 9).toFloat() * 10;
        
        gpsDatum.source = GGA;
        return gpsDatum;
    }
    
    if(NMEAtype == "GPRMC")
    {
        if(GetNMEASubstring(nmeaStr, 2) != String('A')) return gpsDatum;
        //else gpsDatum.gpsFix = 1;
        
        gpsDatum.lat = ConvertToDMM(GetNMEASubstring(nmeaStr, 3));
        if(GetNMEASubstring(nmeaStr, 4) == String('S')) gpsDatum.lat *= -1;
        
        gpsDatum.lon = ConvertToDMM(GetNMEASubstring(nmeaStr, 5));
        if(GetNMEASubstring(nmeaStr, 6) == String('W')) gpsDatum.lon *= -1;
        
        //gpsDatum.speed = GetNMEASubstring(nmeaStr, 7).toFloat();
        
        gpsDatum.NMEAtoTime(GetNMEASubstring(nmeaStr, 1));
        gpsDatum.NMEAtoDate(GetNMEASubstring(nmeaStr, 9));
        
        gpsDatum.source = RMC;
        return gpsDatum;
    }
    
    return 0;
}

String GPS::GetNMEASubstring(const String& str, int commaIndex)
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

long GPS::ConvertToDMM(const String& degStr)
{
    int iDecimal = degStr.indexOf('.');
    if(iDecimal == -1) return 0;
    
    long dmm = degStr.substring(0, iDecimal - 2).toInt() * 600000L;
    dmm += degStr.substring(iDecimal - 2, iDecimal).toInt() * 10000L;
    dmm += degStr.substring(iDecimal + 1).toInt();  //careful -- this assumes it's exactly five digits!!!!!!!!!!!
    
    return dmm;
}
