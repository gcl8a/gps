#ifndef __GPS_H
#define __GPS_H

#include <Arduino.h> // for byte data type

#define GGA 0x01
#define RMC 0x02
#define GSA 0x04
#define GSV 0x08

#define KNOTS_TO_KMH 1.852

//struct GPSDatum
//{
//    uint8_t source = 0; //indicates which strings/readings were used to create it
//
//    GPSDatum(uint8_t s) : source(s) {}
//    uint8_t operator [] (int) {return source;}
//};
//
//struct GGADatum : public GPSDatum
//{
//    GGADatum(void) : GPSDatum(GGA) {}
//
//};
//
//struct RMCDatum : public GPSDatum
//{
//    RMCDatum(void) : GPSDatum(RMC) {}
//
//};

struct GPSDump
{
    uint32_t timestamp = 0;
    
    uint8_t hour, min, sec, msec;
    int32_t lat, lon;
    uint16_t elev; //tenths of a meter -- only goes to 6500 meters

    uint8_t fix = 0;
    uint8_t dummy = 0x55; //to make it an even 20
};

class GPSDatum
{
public:
  uint8_t source = 0; //indicates which strings/readings were used to create it
  
  //unsigned long secondsAfterMidnight;
  uint8_t day = 0, month = 0, year = 0;
  uint8_t hour, minute, second, msec;
  
  long lat = -99;
  long lon = -199;
  float elev = -99;
  float speed = 0;

  uint8_t gpsFix = 0;
    
  uint32_t timestamp = 0; //used to hold value from millis(), not true timestamp

public:
    GPSDatum(uint32_t ts = millis()) : timestamp(ts) {}
    uint8_t operator() (void) {return source;}
    
    uint8_t ParseNMEA(const String& str);
    
    GPSDump MakeDataDump(void)
    {
        GPSDump dump;

        dump.timestamp = timestamp;
        dump.fix = gpsFix;

        dump.hour = hour;
        dump.min = minute;
        dump.sec = second;
        dump.msec = msec;
        
        dump.lat = lat;
        dump.lon = lon;
        
        dump.elev = (uint16_t)(elev * 10);
        
        return dump;
    }
    
    String ParseDataDump(const GPSDump& dump)
    {
        timestamp = dump.timestamp;
        gpsFix = dump.fix;
        
        hour = dump.hour;
        minute = dump.min;
        second = dump.sec;
        msec = dump.msec;
        
        lat = dump.lat;
        lon = dump.lon;
        
        elev = dump.elev / 10.0;
        
        return MakeDataString();
    }
  
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

  int Merge(const GPSDatum& newReading)
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
    
    String MakeDataString(void)
    {
        if(gpsFix)
        {
            char dataStr[100];
            
            sprintf(dataStr, "%lu,%i,%02i:%02i:%02i,%li,%li,%2.1f",
                    timestamp,
//                    source, %X
                    gpsFix,
//                    year,
//                    month,
//                    day,
                    hour,
                    minute,
                    second,
                    lat,
                    lon,
                    elev);
            return String(dataStr);
        }    
        else
            return String("0");
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
        
        sprintf(dataStr, "%02i:%02i:%02i,%2.2f",
//                timestamp,
//                gpsFix,
                hour,
                minute,
                second,
//                lat,
//                lon,
                elev);
        
        return String(dataStr);
    }
};

class GPS
{
protected:
  String gpsString; //working string for storing characters as they roll in across the UART
  HardwareSerial* serial; //UART of choice -- no real need to make it a variable, but so be it

  GPSDatum workingDatum; //working datum; we'll try to add new readings to it and return its state

public:
    GPS(HardwareSerial* ser) : serial(ser) {}

  virtual int Init(void) = 0;
    
    String MakeDataString(void) {return workingDatum.MakeDataString();}

  GPSDatum GetReading(void) {return workingDatum;}

    static uint8_t CalcChecksum(const String& str)
    {
        uint8_t checksum = 0;
        for(uint16_t i = 0; i < str.length(); i++)
        {
            checksum ^= str[i];
        }
        
        return checksum;
    }

    uint8_t CheckSerial(GPSDatum* datum)
    {
        while(serial->available())
        {
            char c = serial->read();
            if(c != '\n' && c != '\r') gpsString += c;  //ignore carriage return and newline
            if(c == '\n') //we have a complete string
            {
                GPSDatum newReading = ParseNMEA(gpsString); //parse it; retVal holds its type
                uint8_t retVal = newReading.source;
                
                if(newReading.source) //if we have a valid string
                {
                    *datum = newReading;
                }
                
                gpsString = "";
                
                return retVal;
            }
        }
        
        return false;
    }
    
    uint8_t CheckSerial(void)
    {
        int retVal = 0;
        while(serial->available())
        {
            char c = serial->read();
            if(c != '\n' && c != '\r') gpsString += c;  //ignore carriage return and newline
            if(c == '\n') //we have a complete string
            {
                //                GPSdatum newReading;
                //                retVal = newReading.ParseNMEA(gpsString); //parse it; retVal holds its type
                GPSDatum newReading = ParseNMEA(gpsString); //parse it; retVal holds its type
                retVal = newReading.source;
                
                if(newReading.source) //if we have a valid string
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
    
    uint8_t CheckSerialNoParse(void)
    {
        while(serial->available())
        {
            char c = serial->read();
            if(c != '\n' && c != '\r') gpsString += c;  //ignore carriage return and newline
            if(c == '\n') //we have a complete string
            {
                return true;
            }
        }
        
        return false;
    }
    
int SendNMEA(const String& str)
{
    if(serial)
    {
        serial->print(MakeNMEAwithChecksum(str));
        return 1;
    }
    return 0;
}
    
    GPSDump MakeDataDump(void) { return workingDatum.MakeDataDump();}


  static String MakeNMEAwithChecksum(const String& str);
    GPSDatum ParseNMEA(const String& nmeaStr);
    GPSDatum ParseNMEA(void) {return ParseNMEA(gpsString);}

protected: //utility functions
    static String GetNMEASubstring(const String& str, int commaIndex);
    static long ConvertToDMM(const String& degStr);
    int NMEAtoTime(const String& timeStr);
    int NMEAtoDate(const String& dateStr);

};

class GPS_EM506 : public GPS
{
  public:
    GPS_EM506(HardwareSerial* ser) : GPS(ser) {}
    int Init(void)
    {
      serial->begin(4800);

//      SendNMEA(F("PSRF103,02,00,00,01"));
//      SendNMEA(F("PSRF103,03,00,00,01"));
//      SendNMEA(F("PSRF103,04,00,01,01"));

        SetActiveNMEAStrings(GGA | RMC);

      return 1;
    }
    

//    bool SetReportPeriod(uint16_t per)
//    {
//        char str[24];
//        sprintf(str, "PMTK220,%i", per);
//        SendNMEA(str);
//
//        //should really wait for confirmation
//        return true;
//    }
//
    bool SetActiveNMEAStrings(uint8_t strings)
    {
        char str[96];

        sprintf(str, "PSRF103,00,00,%2i,01", strings & GGA ? 1 : 0);
        SendNMEA(str);

        sprintf(str, "PSRF103,02,00,%2i,01", strings & GSA ? 1 : 0);
        SendNMEA(str);
        
        sprintf(str, "PSRF103,03,00,%2i,01", strings & GSV ? 1 : 0);
        SendNMEA(str);
        
        sprintf(str, "PSRF103,04,00,%2i,01", strings & RMC ? 1 : 0);
        SendNMEA(str);

        //should really wait for confirmation
        return true;
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
