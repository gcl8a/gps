#ifndef __GPS_H
#define __GPS_H

#include <Arduino.h> // for byte data type

#define GGA 0x01
#define RMC 0x02

#define KNOTS_TO_KMH 1.852

class GPSdatum
{
protected:
  uint8_t source = 0; //indicates which strings/readings were used to create it
  
  //unsigned long secondsAfterMidnight;
  uint8_t day, month, year;
  uint8_t hour, minute, second, msec;
  
  long lat = -99;
  long lon = -199;
  float elev = -99; //not really a double, but sprintf balks otherwise
  float speed = 0;

  uint8_t gpsFix = 0;
    
  uint32_t timestamp = 0; //used to hold value from millis(), not true timestamp

public:
    
  GPSdatum(uint32_t ts = 0) : timestamp(ts) {}
    
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

  int Merge(const GPSdatum& newReading)
  {
    int retVal = 0;
    if(newReading.hour == hour && newReading.minute == minute && newReading.second == second && newReading.msec == msec)
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
    
//    String MakeShortDataString(void)
//    {
////        if(gpsFix)
//        {
//            char dataStr[100];
//
//            sprintf(dataStr, "%lu,%i,%02i:%02i:%02i,%li,%li,%li,%li",
//                    timestamp,
//                    gpsFix,
//                    hour,
//                    minute,
//                    second,
//                    lat,
//                    lon,
//                    long(elev*10),
//                    long(speed*KNOTS_TO_KMH*10));
//
//            return String(dataStr);
//        }
////        else
////            return String();
//    }
    
    String MakeShortDataString(void)
    {
        char dataStr[100];
        
        sprintf(dataStr, "%i,%02i:%02i:%02i",
//                timestamp,
                gpsFix,
                hour,
                minute,
                second);
//                lat,
//                lon,
//                elev);
        
        return String(dataStr) + ',' + String(elev);
    }
};

class GPS
{
protected:
  String gpsString; //working string for storing characters as they roll in across the UART
  HardwareSerial* serial; //UART of choice -- no real need to make it a variable, but so be it

  GPSdatum workingDatum; //working datum; we'll try to add new readings to it and return its state

public:
    GPS(HardwareSerial* ser) : serial(ser) {}

  virtual int Init(void) = 0;

  GPSdatum GetReading(void) {return workingDatum;}

  int CheckSerial(void)
  {
    int retVal = 0;
    while(serial->available())
    {
      char c = serial->read();
      if(c != '\n' && c != '\r') gpsString += c;  //ignore carriage return and newline
      if(c == '\n') //we have a complete string
      {
        GPSdatum newReading;
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
        SetReportPeriod(1000); //rate, in ms; default to 1 Hz
        SetActiveNMEAStrings(GGA | RMC);
        //SendNMEA("PMTK401");
      //SendNMEA("PMTK413");
      
      return 1;
    }
    
    bool SetReportPeriod(uint16_t per)
    {
        char str[24];
        sprintf(str, "PMTK220,%i", per);
        SendNMEA(str);
        
        //should really wait for confirmation
        return true;
    }
    
    bool SetActiveNMEAStrings(uint8_t strings)
    {
        char str[96];
        sprintf(str, "PMTK314,0,%i,0,%i,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0", strings & RMC ? 1 : 0, strings & GGA ? 1 : 0);
        SendNMEA(str);
        
        //should really wait for confirmation
        return true;
    }
};

#endif
