/*
 * Forked code for SAMD21 dev. Makes a "traditional" SPI on sercom 1 (Arduino pins 11, 12, 13):
 * 
 * 11: MOSI on sercom 1:0
 * 12: MISO on sercom 1:3
 * 13: CLK on sercom 1:1
 * 
 * When it first makes connection over GPS, it uses the month and data
 * to create a filename, then writes data to it. Multiple files on the same day are numbered 
 * sequentially.
 */

//#include "wiring_private.h" // pinPeripheral() function

#include <gps.h>

/*
 * setup serial for the GPS on SERCOM2: 
 * Arduino pin 3 -> sercom 2:1* -> RX
 * Arduino pin 4 -> sercom 2:0* -> TX
 */
//Uart gpsSerial (&sercom2, 3, 4, SERCOM_RX_PAD_1, UART_TX_PAD_0);

GPS_EM506 gps(&Serial1);

//void SERCOM2_Handler()
//{
//  gpsSerial.IrqHandler();
//}

void setup() 
{
  delay(2000);
  SerialUSB.begin(115200);
  SerialUSB.println("Hej.");

  
  //assign pins 3 & 4 SERCOM functionality
//  gpsSerial.begin(9600);
//  pinPeripheral(3, PIO_SERCOM_ALT);
//  pinPeripheral(4, PIO_SERCOM_ALT);

  delay(2000);

  gps.Init();
  
  SerialUSB.println(F("Setup complete."));

  SerialUSB.println(F("Checking for signal."));
  while(!(gps.CheckSerial() & RMC) && !SerialUSB.available()) {}

  GPSDatum gpsDatum = gps.GetReading();  
  
  //now we have a fix -- make a filename

  //no longer need RMC
  //gps.SetActiveNMEAStrings(GGA);
  //gps.SetReportTime(2000);
  //gps.SendNMEA("PMTK314,0,5,0,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0");

  SerialUSB.println(F("Done."));
}

void loop() 
{
  if(gps.CheckSerial() & GGA) //if we have a GGA
  {
    String reportStr = gps.GetReading().MakeDataString();

    SerialUSB.println(reportStr);  
  }

}

