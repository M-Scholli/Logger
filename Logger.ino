/*
 SD card datalogger

 This example shows how to log data from three analog sensors
 to an SD card using the SD library.

 The circuit:
 * SD card attached to SPI bus as follows:
 ** UNO:  MOSI - pin 11, MISO - pin 12, CLK - pin 13, CS - pin 4 (CS pin can be changed)
 and pin #10 (SS) must be an output
 ** Mega:  MOSI - pin 51, MISO - pin 50, CLK - pin 52, CS - pin 4 (CS pin can be changed)
 and pin #52 (SS) must be an output
 ** Leonardo: Connect to hardware SPI via the ICSP header
 Pin 4 used here for consistency with other Arduino examples

 created  24 Nov 2010
 modified 9 Apr 2012 by Tom Igoe

 This example code is in the public domain.

 */

#include <SPI.h>
#include <SD.h>
#include <Wire.h>    // I2C-Bibliothek einbinden
#include "RTClib.h"  // RTC-Bibliothek einbinden
#include "floatToString.h"

RTC_DS1307 RTC;      // RTC Modul

// On the Ethernet Shield, CS is pin 4. Note that even if it's not
// used as the CS pin, the hardware CS pin (10 on most Arduino boards,
// 53 on the Mega) must be left as an output or the SD library
// functions will not work.
const int chipSelect = 53;

File dataFile;

void
setup ()
{

  // Initialisiere I2C
  Wire.begin ();

  // Initialisiere RTC
  RTC.begin ();

  // Open serial communications and wait for port to open:
  Serial.begin (9600);
  while (!Serial)
    {
      ; // wait for serial port to connect. Needed for Leonardo only
    }

  // Begrüßungstext auf seriellem Monitor ausgeben
  Serial.println ("Starte Datum und Zeit");

  if (!RTC.isrunning ())
    {

      // Aktuelles Datum und Zeit setzen, falls die Uhr noch nicht läuft
      RTC.adjust (DateTime (2000, 0, 0, 0, 0, 0));

      Serial.println (
	  "Echtzeituhr wurde gestartet und auf Systemzeit gesetzt.");
    }
  else
    Serial.println ("Echtzeituhr laeuft bereits.");

  Serial.print ("Initializing SD card...");
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode (SS, OUTPUT);

  // see if the card is present and can be initialized:
  if (!SD.begin (chipSelect))
    {
      Serial.println ("Card failed, or not present");
      // don't do anything more:
      while (!SD.begin (chipSelect))
	{
	  delay (500);
	}
    }
  Serial.println ("card initialized.");

  DateTime now = RTC.now ();
  String logName = "";
  logName += String (now.day ());
  logName += "-";
  logName += String (now.month ());
  logName += "-";
  if (now.year () - 2000 <= 99)
    {
      logName += String (now.year () - 2000);
    }
  else
    logName += "EE";
  logName += ".csv";
  char charFileName[logName.length () + 1];
  logName.toCharArray (charFileName, sizeof(charFileName));
  Serial.println (charFileName);
  // Open up the file we're going to log to!
  dataFile = SD.open (charFileName, FILE_WRITE);
  if (!dataFile)
    {
      Serial.println ("error opening datalog.txt");
      // Wait forever since we cant write data
      while (1)
	;
    }
  dataFile.println ("Datum;Uhrzeit;Sensor1;Sensor2;Sensor3");
  dataFile.println ("dd.mm.yyyy;hh.mm.ss;Grad C;Sensor2;Sensor3");
}

void
loop ()
{
  DateTime now = RTC.now (); // aktuelle Zeit abrufen

  show_time_and_date (now);  // Datum und Uhrzeit ausgeben

  // make a string for assembling the data to log:
  String dataString = "";

  dataString += date_string (now);
  dataString += ";";
  dataString += time_string (now);
  dataString += ";";

  // read three sensors and append to the string:
  dataString += read_temp (5);
  dataString += ";";
  dataString += String (analogRead (A5));

  dataFile.println (dataString);

  // print to the serial port too:
  Serial.println (dataString);

  // The following line will 'save' the file to the SD card after every
  // line of data - this will use more power and slow down how much data
  // you can read but it's safer!
  // If you want to speed up the system, remove the call to flush() and it
  // will save the file only every 512 bytes - every time a sector on the
  // SD card is filled with data.
  dataFile.flush ();

  // Take 1 measurement every 500 milliseconds
  delay (2000);
}

//Messe Temp. 0V=0gradC 5V=100gradC

String
read_temp (int pin)
{
  String temp;
  char buffer[8];
  float v = analogRead (pin);
  float t = 100 * v / 1023;
  temp = floatToString (buffer, t, 2, true);
  return temp;
}

// Wochentag ermitteln
String
get_day_of_week (uint8_t dow)
{

  String dows = "  ";
  switch (dow)
    {
    case 0:
      dows = "So";
      break;
    case 1:
      dows = "Mo";
      break;
    case 2:
      dows = "Di";
      break;
    case 3:
      dows = "Mi";
      break;
    case 4:
      dows = "Do";
      break;
    case 5:
      dows = "Fr";
      break;
    case 6:
      dows = "Sa";
      break;
    }

  return dows;
}

// Datums String
String
date_string (DateTime datetime)
{
  String s = "";
  s += String (datetime.day ());
  s += ".";
  s += String (datetime.month ());
  s += ".";
  s += String (datetime.year ());
  return s;
}

// Datums String
String
time_string (DateTime datetime)
{
  String s = "";
  s += String (datetime.hour ());
  s += ":";
  s += String (datetime.minute ());
  s += ":";
  s += String (datetime.second ());
  return s;
}

// Datum und Uhrzeit ausgeben
void
show_time_and_date (DateTime datetime)
{

  // Wochentag, Tag.Monat.Jahr
  Serial.print (get_day_of_week (datetime.dayOfWeek ()));
  Serial.print (", ");
  if (datetime.day () < 10)
    Serial.print (0);
  Serial.print (datetime.day (), DEC);
  Serial.print (".");
  if (datetime.month () < 10)
    Serial.print (0);
  Serial.print (datetime.month (), DEC);
  Serial.print (".");
  Serial.println (datetime.year (), DEC);

  // Stunde:Minute:Sekunde
  if (datetime.hour () < 10)
    Serial.print (0);
  Serial.print (datetime.hour (), DEC);
  Serial.print (":");
  if (datetime.minute () < 10)
    Serial.print (0);
  Serial.print (datetime.minute (), DEC);
  Serial.print (":");
  if (datetime.second () < 10)
    Serial.print (0);
  Serial.println (datetime.second (), DEC);
}
