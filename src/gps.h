#ifndef __GPS_H
#define __GPS_H

#include <Arduino.h> // for byte data type
#include <vector_uC.h>

#define GGA 0x01
#define RMC 0x02
#define GSA 0x04
#define GSV 0x08

#define KNOTS_TO_KMH 1.852001

enum GPS_PROTOCOL {GPS_NMEA = 1, GPS_BINARY};
enum MESSAGE_STATE {WAITING0, WAITING1, SIZE0, SIZE1, PAYLOAD, CHECK0, CHECK1, CLOSE0, CLOSE1, COMPLETE, EPILOG_ERROR = 253, CHECKSUM_ERROR = 254, ERROR = 255};

#define vector_u8 TVector<uint8_t>

class GPSMessage //binary messages; really a struct?
{
protected:
    uint16_t index = 0; //to keep track of additions
    uint16_t checksum = 0x8000;
    uint16_t runningSum = 0;
    
public:
    GPSMessage(uint16_t size) : payload(size) {index = 0;}
    
    uint8_t msgID = 0;
    vector_u8 payload; //includes MID as byte 0
    
    uint16_t AddByteToPayload(uint8_t b)
    {
        if(index == 0) msgID = b; //double storing, but it also acts as an indicator
        
        if(index < payload.Length())
        {
            payload[index++] = b;
            runningSum += b;
        }
        
        return index;
    }
    
    uint16_t AddChecksum(uint8_t b)
    //bit 16 is always 0 in the checksum, so we'll use that to indicate which byte we're working on
    {
        if(checksum & 0x8000) //first byte
            checksum = ((uint16_t)b << 8);
        else
            checksum |= b;
        
        return checksum ^ (runningSum & 0x7fff); //zero when checksum is correct
    }
};

class GPSDatum //really GGA now...
{
public:
  uint8_t source = 0; //indicates which strings/readings were used to create it

  uint8_t day = 0, month = 0, year = 0;
    uint8_t hour, minute, second, msec;
  
  int32_t lat = -99;
  int32_t lon = -199;
  int16_t elevDM = -99; //elevation is stored as dm to save space
  //float speed = 0;

  uint16_t gpsFix = 0; //2-bytes to make size % 4

  uint32_t timestamp = 0; //used to hold value from millis(), not true timestamp

public:
    GPSDatum(uint32_t ts = millis()) : timestamp(ts) {}
    uint8_t operator() (void) {return source;}
    
    uint8_t ParseNMEA(const String& str);
    
  static String GetNMEASubstring(const String& str, int commaIndex);
  static long ConvertToDMM(const String& degStr);
  int NMEAtoTime(const String& timeStr);
  int NMEAtoDate(const String& dateStr);

//    static uint8_t CalcChecksum(const String& str)
//    {
//        uint8_t checksum = 0;
//        for(uint16_t i = 0; i < str.length(); i++)
//        {
//            checksum ^= str[i];
//        }
//
//        return checksum;
//    }
//
//    static uint16_t CalcChecksumBinary(uint8_t* msg, uint16_t len)
//    {
//        uint16_t checksum = 0;
//        for(uint16_t i = 0; i < len; i++)
//        {
//            checksum += msg[i];
//        }
//
//        checksum &= 0x7fff; //unclear if we're supposed to do this every add or not, but it would only matter for an unrealistically long messages
//
//        return checksum;
//    }
    
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

        //speed = newReading.speed;
      }

      else if(newReading.source & GGA)
      {
        elevDM = newReading.elevDM;
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
                    (elevDM / 10.0));
            return String(dataStr);
        }    
        else
            return String("0");
    }
    
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
                (elevDM / 10.0));
        
        return String(dataStr);
    }
};

class GPS
{
protected:
    GPS_PROTOCOL gpsProtocol = GPS_NMEA;
    
    String gpsString; //working string for storing characters as they roll in across the UART
    HardwareSerial* serial; //UART of choice -- no real need to make it a variable, but so be it

    GPSDatum workingDatum; //working datum; we'll try to add new readings to it and return its state

    GPSMessage gpsMessage; //used for holding serial data as it comes in; only binary for now
public:
    GPS(HardwareSerial* ser, GPS_PROTOCOL p) : serial(ser), gpsMessage(0)
    {
        gpsProtocol = p;
    }

  virtual int Init(void) = 0;
    
    String MakeDataString(void) {return workingDatum.MakeDataString();}

    vector_u8 GetMessage(void) {return gpsMessage.payload;}
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

    static uint16_t CalcChecksumBinary(uint8_t* msg, uint16_t len)
    {
        uint16_t checksum = 0;
        for(uint16_t i = 0; i < len; i++)
        {
            checksum += msg[i];
        }

        checksum &= 0x7fff; //unclear if we're supposed to do this every add or not, but it would only matter for an unrealistically long messages

        return checksum;
    }
    
    uint8_t QueryPower(void)
    {
        uint8_t msg[] = {209, 218, 0};
        
        SendBinary(msg, 3);
        
        return 0;
    }
    
    uint8_t QueryMID(uint8_t mid)
    {
        uint8_t msg_req[] = {166, 0x01, mid, 0x00, 0x00, 0x00, 0x00, 0x00};
        SendBinary(msg_req, 8);
        
        return mid;
    }
    
    uint8_t EnableNavDebug(void)
    {
        uint8_t msg_req[] = {166, 5, 0, 2, 0, 0, 0, 0};
        SendBinary(msg_req, 8);
        
        return 0;
    }
    
    uint8_t RequestMID(uint8_t mid)
    {
        uint8_t msg[] = {168, mid};
        
        SendBinary(msg, 2);
        
        return mid;
    }
    
    uint8_t PollPowerMode(void)
    {
        uint8_t msg[] = {233, 11};
        
        SendBinary(msg, 2);
        
        return 0;
    }
    
    uint8_t CheckSerialBinary(void)
    {
        static MESSAGE_STATE msgState = WAITING0;
        static uint16_t msgLen = 0;
        
        if(msgState == COMPLETE) msgState = WAITING0; //kludge...
        
        while(serial->available())
        {
            uint8_t b = serial->read();
            //SerialUSB.print(b, HEX);
            
            switch(msgState)
            {
                case WAITING0:
                    if(b == 0xa0) msgState = WAITING1;
                    break;
                case WAITING1:
                    if(b == 0xA2) msgState = SIZE0;
                    break;
                case SIZE0:
                    msgLen = (uint16_t)b << 8;
                    msgState = SIZE1;
                    break;
                case SIZE1:
                    msgLen += b;
                    gpsMessage = GPSMessage(msgLen); //could be faster, long time copying...
                    msgState = PAYLOAD;
                    break;
                case PAYLOAD:
                {
                    uint16_t index = gpsMessage.AddByteToPayload(b);
                    if(index == msgLen) msgState = CHECK0;
                }
                    break;
                case CHECK0:
                    gpsMessage.AddChecksum(b);
                    msgState = CHECK1;
                    break;
                case CHECK1:
                {
                    uint16_t cs = gpsMessage.AddChecksum(b);
                    if(cs)
                    {
                        msgState = WAITING0; //failed checksum
                        return CHECKSUM_ERROR;
                    }
                    
                    else msgState = CLOSE0;
                }
                    break;
                case CLOSE0:
                    if(b != 0xB0)
                    {
                        msgState = WAITING0; //failed checksum
                        return EPILOG_ERROR;
                    }
                    
                    else msgState = CLOSE1;
                    break;
                case CLOSE1:
                    if(b != 0xB3)
                    {
                        msgState = WAITING0; //failed checksum
                        return EPILOG_ERROR;
                    }
                    
                    else return msgState = COMPLETE; //need to return since we're only doing one at a time for now
                case COMPLETE:
                    msgState = WAITING0; //assume it's been processed
                    break;
                default:
                    msgState = ERROR;
            }
        }
        
        return msgState;
    }
    
    uint8_t CheckSerial(void)
    {
        int retVal = 0;
        while(serial->available())
        {
            char c = serial->read();
            SerialUSB.print(c);
            
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
    
    int SendNMEA(const String& str)
    {
        //SerialMonitor.print(MakeNMEAwithChecksum(str));
        if(serial)
        {
            serial->print(MakeNMEAwithChecksum(str));
            return 1;
        }
        
        return 0;
    }
    
    int SendBinary(uint8_t* message, uint16_t length) //can technically be longer than 256, but that's probably a bad idea...
    {
        uint8_t buffer[length + 8];
        
        //mid is included in the checksum
        uint16_t checksum = CalcChecksumBinary(message, length);
        
        //header
        buffer[0] = 0xA0;
        buffer[1] = 0xA2;
        
        //big-endian
        buffer[2] = (uint8_t)(length >> 8);
        buffer[3] = (uint8_t)(length & 0xff);
        
        //the message, including mid
        for(uint16_t i = 0; i < length; i++)
        {
            buffer[i + 4] = message[i];
        }
        
        //checksum, big-endian
        buffer[length + 4] = (uint8_t)(checksum >> 8);
        buffer[length + 5] = (uint8_t)(checksum);
        
        buffer[length + 6] = 0xB0;
        buffer[length + 7] = 0xB3;
        
        //now send the whole package
        for(uint16_t i = 0; i < length + 8; i++)
        {
            //SerialMonitor.print(buffer[i], HEX);
            if(serial) serial->write(buffer[i]);
        }
        
        return 0;
    }
    
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
    GPS_EM506(HardwareSerial* ser, GPS_PROTOCOL p = GPS_NMEA) : GPS(ser, p) {}
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

        sprintf(str, "PSRF103,00,00,%02i,01", strings & GGA ? 1 : 0);
        SendNMEA(str);

        sprintf(str, "PSRF103,02,00,%02i,01", strings & GSA ? 1 : 0);
        SendNMEA(str);
        
        sprintf(str, "PSRF103,03,00,%02i,01", strings & GSV ? 1 : 0);
        SendNMEA(str);
        
        sprintf(str, "PSRF103,04,00,%02i,01", strings & RMC ? 1 : 0);
        SendNMEA(str);

        //should really wait for confirmation
        return true;
    }
};

class GPS_MTK3339 : public GPS
{
public:
    GPS_MTK3339(HardwareSerial* ser, GPS_PROTOCOL p = GPS_NMEA) : GPS(ser, p) {}
    int Init(void)
    {
        serial->begin(9600);

        //SetReportPeriod(1000); //rate, in ms; default to 1 Hz
        //SetActiveNMEAStrings(GGA | RMC);
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

class GPS_GP_735 : public GPS
{
public:
    GPS_GP_735(HardwareSerial* ser, GPS_PROTOCOL p = GPS_NMEA) : GPS(ser, p) {}
    int Init(void)
    {
        serial->begin(9600);
        //SetReportPeriod(1000); //rate, in ms; default to 1 Hz
        //SetActiveNMEAStrings(GGA | RMC);
        //SendNMEA("PMTK401");
        //SendNMEA("PMTK413");
        
        return 1;
    }
};

class GPS_JF2 : public GPS
{
protected:
    const uint8_t GPS_ONOFFPin = A3;
    const uint8_t GPS_SYSONPin = A2;
    const uint8_t RXPin = 0;
    const uint8_t TXPin = 1;
    const uint32_t GPSBaud = 9600;

public:
    GPS_JF2(HardwareSerial* ser, GPS_PROTOCOL p = GPS_NMEA) : GPS(ser, p) {}
    int Init(void)
    {
        serial->begin(GPSBaud);
        while(!*serial) {}

        pinMode(GPS_SYSONPin, INPUT);
        digitalWrite(GPS_ONOFFPin, LOW);

        pinMode(GPS_ONOFFPin, OUTPUT);
        
        delay(100);
        
        while (digitalRead( GPS_SYSONPin ) == LOW )
        {
            // Need to wake the module
            digitalWrite( GPS_ONOFFPin, HIGH );
            delay(5);
            digitalWrite( GPS_ONOFFPin, LOW );
            delay(100);
        }
        
        //delay(500);
        //SetReportPeriod(1000); //rate, in ms; default to 1 Hz
        //SetActiveNMEAStrings(GGA | RMC);
        //SendNMEA("PMTK401");
        //SendNMEA("PMTK413");
        
        return 1;
    }
    
    bool SetActiveNMEAStrings(uint8_t strings)
    {
        char str[96];
        
        sprintf(str, "PSRF103,00,00,%02i,01", strings & GGA ? 1 : 0);
        SendNMEA(str);
        
        sprintf(str, "PSRF103,02,00,%02i,01", strings & GSA ? 1 : 0);
        SendNMEA(str);
        
        sprintf(str, "PSRF103,03,00,%02i,01", strings & GSV ? 1 : 0);
        SendNMEA(str);
        
        sprintf(str, "PSRF103,04,00,%02i,01", strings & RMC ? 1 : 0);
        SendNMEA(str);
        
        //should really wait for confirmation
        return true;
    }
    
    uint8_t SetProtocol(GPS_PROTOCOL protocol)
    {
        if(gpsProtocol == GPS_NMEA) //we're in NMEA mode
        {
            char str[96];
            sprintf(str, "PSRF100,%i,9600,8,1,0", protocol == GPS_BINARY ? 0 : 1);
            SendNMEA(str);
            
            gpsProtocol = (protocol == GPS_BINARY ? GPS_BINARY : GPS_NMEA);
        }
        
        else if(gpsProtocol == GPS_BINARY) //we're in binary mode
        {
            uint8_t msg[] = {0x87, 0x02};
            
            SendBinary(msg, 2);
            
            gpsProtocol = (protocol == GPS_BINARY ? GPS_BINARY : GPS_NMEA);
        }
        
        return gpsProtocol;
    }
    
    int8_t SetSBAS(void) //only one option with this device
    {
        if(gpsProtocol != GPS_BINARY) return -1;
        
        uint8_t msg[] = {0x85, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
        SendBinary(msg, 7);
        
        delay(1000);
        uint8_t msg_req[] = {0xA6, 0x00, 0x32, 0x01, 0x00, 0x00, 0x00, 0x00};
        SendBinary(msg_req, 8);
        
        delay(1000);
        uint8_t msg_req27[] = {0xA6, 0x00, 27, 0x01, 0x00, 0x00, 0x00, 0x00};
        SendBinary(msg_req27, 8);
        
        delay(1000);
        uint8_t msg_req29[] = {0xA6, 0x00, 29, 0x01, 0x00, 0x00, 0x00, 0x00};
        SendBinary(msg_req29, 8);
        
        delay(1000);
        uint8_t sbasParammsg[] = {170, 0, 0, 0, 0, 0};
        SendBinary(sbasParammsg, 6);
        
        //delay(1000);
        //uint8_t msg_req[] = {0xA6, 0x01, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00};
        //SendBinary(msg_req, 8);
        
        return 1;
    }
    
    void RequestFullPower(void)
    {
        uint8_t msg[] = {0xda, 0};
        SendBinary(msg, 2);
    }

    int8_t RequestTricklePower(void)
    {
        if(gpsProtocol != GPS_BINARY) return -1;
        
        uint8_t pwrMsg[16];
        
        pwrMsg[0] = 0xda; //power
        pwrMsg[1] = 0x03; //trickle power
        
        uint16_t dutyCycle = 100;
        memcpy(&pwrMsg[2], &dutyCycle, 2);

        uint32_t periodOn = 400;
        memcpy(&pwrMsg[4], &periodOn, 4);

        uint32_t maxOff = 15000; //default is 30000
        memcpy(&pwrMsg[8], &maxOff, 4);
        
        uint32_t maxSearchMS = 120000; //default
        memcpy(&pwrMsg[12], &maxSearchMS, 4);

        SendBinary(pwrMsg, 16);
        
        return 1;
    }
};

#endif
