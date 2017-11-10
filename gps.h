#ifndef __GPS_H
#define __GPS_H

#define GGA 0x01
#define RMC 0x02

#define KNOTS_TO_KMH 1.852

class GPSreading
{
protected:
  uint8_t source = 0; //indicates which strings/readings were used to create it
  
  //unsigned long secondsAfterMidnight;
  uint8_t day, month, year;
  uint8_t hour, minute, second;
  
  long lat = -99;
  long lon = -199;
  double elev = -99; //not really a double, but sprintf balks otherwise
  double speed = 0;

  uint8_t gpsFix = 0;
  uint32_t timestamp = 0; //used to hold value from millis(), not true timestamp

public:
    GPSreading(uint32_t ts = 0) : timestamp(ts) {}
    
  uint8_t ParseNMEA(const String& str);
  
  static String GetNMEASubstring(const String& str, int commaIndex);
  static long ConvertToDMM(const String& degStr);
  int NMEAtoTime(const String& timeStr);
  int NMEAtoDate(const String& dateStr);
  static uint8_t CalcChecksum(const String& str)
  {
    uint8_t checksum = 0;
    for(uint16_t i = 0; i < str.length(); i++)
    {
      checksum ^= str[i];
    }

    return checksum;
  }

  int Merge(GPSreading newReading)
  {
    int retVal = 0;
    if(newReading.hour == hour && newReading.minute == minute && newReading.second == second)
    {
      if(newReading.source & RMC)
      {
        year = newReading.year;
        month = newReading.month;
        day = newReading.day;

        speed = newReading.speed;
      }

      else if(newReading.source & GGA)
      {
        elev = newReading.elev;
        gpsFix = newReading.gpsFix;
      }

      source |= newReading.source;
      retVal = source; 
    }

    return retVal;
  }
    
    int MakeMonthDay(void)
    {
        return month * 100 + day;
    }

    String MakeDataString(void)
    {
        if(gpsFix)
        {
            char dataStr[100];
            
            sprintf(dataStr, "%lu,%X,%i,20%02i/%02i/%02i,%02i:%02i:%02i,%li,%li,%li,%li",
                    timestamp,
                    source,
                    gpsFix,
                    year,
                    month,
                    day,
                    hour,
                    minute,
                    second,
                    lat,
                    lon,
                    long(elev*10),
                    long(speed*KNOTS_TO_KMH*10));
            
            return String(dataStr);
        }    
        else
            return String(); 
    }
    
    String MakeShortDataString(void)
    {
        if(gpsFix)
        {
            char dataStr[100];
            
            sprintf(dataStr, "%i,%02i:%02i:%02i,%li,%li,%li,%li",
                    gpsFix,
                    hour,
                    minute,
                    second,
                    lat,
                    lon,
                    long(elev*10),
                    long(speed*KNOTS_TO_KMH*10));
            
            return String(dataStr);
        }    
        else
            return String(); 
    }
};

uint8_t GPSreading::ParseNMEA(const String& nmeaStr) 
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

String GPSreading::GetNMEASubstring(const String& str, int commaIndex)
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

long GPSreading::ConvertToDMM(const String& degStr)
{
  int iDecimal = degStr.indexOf('.');
  if(iDecimal == -1) return 0;

  long dmm = degStr.substring(0, iDecimal - 2).toInt() * 600000L;
  dmm += degStr.substring(iDecimal - 2, iDecimal).toInt() * 10000L;
  dmm += degStr.substring(iDecimal + 1).toInt();

  return dmm;
}

int GPSreading::NMEAtoTime(const String& timeStr)
{
  if(timeStr.length() != 10) return 0;

  hour = timeStr.substring(0, 2).toInt();
  minute = timeStr.substring(2, 4).toInt();
  second = timeStr.substring(4, 6).toInt();

  return 1;
}

int GPSreading::NMEAtoDate(const String& dateStr)
{
  if(dateStr.length() != 6) return 0;

  day = dateStr.substring(0, 2).toInt();
  month = dateStr.substring(2, 4).toInt();
  year = dateStr.substring(4, 6).toInt();
  
  return 1;
}

class GPS
{
protected:
  String gpsString; //working string for storing characters as they roll in across the UART
  HardwareSerial* serial; //UART of choice -- no real need to make it a variable, but so be it

  GPSreading workingDatum; //working datum; we'll try to add new readings to it and return its state

public:
    GPS(HardwareSerial* ser) : serial(ser) {}

  virtual int Init(void) = 0;

  GPSreading GetReading(void) {return workingDatum;}

  int CheckSerial(void)
  {
    int retVal = 0;
    while(serial->available())
    {
      char c = serial->read();
      if(c != '\n' && c != '\r') gpsString += c;  //ignore carriage return and newline
      if(c == '\n') //we have a complete string
      {
        GPSreading newReading;
        retVal = newReading.ParseNMEA(gpsString); //parse it; retVal holds its type
          
        if(retVal) //if we have a valid string
        {
          //first, try to combine it with the previous
          int combined = workingDatum.Merge(newReading);
          if(combined) //it worked
          {
            retVal = combined;
          }
          else //the two don't merge
          {
            //start anew with the new datum
            workingDatum = newReading;
          }
        }
        gpsString = "";
      }
    }

    return retVal;
  }

  int SendNMEA(const String& str)
  {
    serial->print(MakeNMEAwithChecksum(str));
    return 1;
  }

  static String MakeNMEAwithChecksum(const String& str);
};

String GPS::MakeNMEAwithChecksum(const String& str)
{
  String checkSum = String(GPSreading::CalcChecksum(str), HEX);
  checkSum.toUpperCase();
  return '$' + str + '*' + checkSum + "\r\n";
}

class GPS_EM506 : public GPS
{
  public:
    GPS_EM506(HardwareSerial* ser) : GPS(ser) {}
    int Init(void)
    {
      serial->begin(4800);

      SendNMEA(F("PSRF103,02,00,00,01"));
      SendNMEA(F("PSRF103,03,00,00,01"));
      SendNMEA(F("PSRF103,04,00,01,01"));

      return 1;
    }
};

class GPS_MTK3339 : public GPS
{
  public:
    GPS_MTK3339(HardwareSerial* ser) : GPS(ser) {}
    int Init(void)
    {
      //serial->begin(9600);
      //delay(100);
        SendNMEA("PMTK220,2000");
      SendNMEA("PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0");
      //SendNMEA("PMTK401");
      //SendNMEA("PMTK413");
      
      return 1;
    }
};

#endif
