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
#include <avr/pgmspace.h>
#include <SPI.h>
#include <Sdfat.h>
#include <Wire.h>    // I2C-Bibliothek einbinden
#include "RTClib.h"  // RTC-Bibliothek einbinden
#include "floatToString.h"
#include "OneWire.h"
#include "DallasTemperature.h"
#include <LiquidCrystal.h>
#include <MemoryFree.h>
#include "Timer.h"
//#include "SdFatUtil.h"

#define LOGTIME 6000 	//Zeit zwischen den Messungen in ms;
#define LCDTIME 300
#define ADCPIN	A7	// Pin an dem der Temperatursensor 1 angeschlossen ist

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd (2, 3, 4, 5, 6, 7, 8);

#define ONE_WIRE_BUS A1

OneWire ourWire (ONE_WIRE_BUS); /* Ini oneWire instance */

DallasTemperature sensors (&ourWire);/* Dallas Temperature Library für Nutzung der oneWire Library vorbereiten */

RTC_DS1307 RTC;      // RTC Modul

//DeviceAdressen der einzelnen ds1820 Temperatursensoren angeben. (loop anpassen)
DeviceAddress sensorsa[] =
  {
    { 0x28, 0xC9, 0xFA, 0xF8, 0x4, 0x0, 0x0, 0xFF },
    { 0x28, 0x58, 0xE4, 0xF8, 0x4, 0x0, 0x0, 0xA7 },
    { 0x28, 0x66, 0x4F, 0xF9, 0x4, 0x0, 0x0, 0x95 },
    { 0x28, 0x3F, 0x77, 0xF9, 0x4, 0x0, 0x0, 0x97 } };

void
dateTime (uint16_t* date, uint16_t* time)
{
  DateTime now = RTC.now ();

  // return date using FAT_DATE macro to format fields
  *date = FAT_DATE (now.year (), now.month (), now.day ());

  // return time using FAT_TIME macro to format fields
  *time = FAT_TIME (now.hour (), now.minute (), now.second ());
}

// Timer für das Aufzeichnungsintervall
Timer tLoop;

// On the Ethernet Shield, CS is pin 4. Note that even if it's not
// used as the CS pin, the hardware CS pin (10 on most Arduino boards,
// 53 on the Mega) must be left as an output or the SD library
// functions will not work.
const uint8_t chipSelect = 10;

SdFat SD;
SdFile dataFile;

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

  lcd.begin (20, 4);

  // Begrüßungstext auf seriellem Monitor und Display ausgeben
  lcd.clear ();
  lcd.print (F("FVA Datenlogger"));
  Serial.println (freeMemory ());
  delay (300);
  lcd.setCursor (0, 1);
  lcd.print (F("Starte RTC"));
  lcd.setCursor (0, 2);
  lcd.print (F("Init. SD Karte"));
  delay (1000);

  if (!RTC.isrunning ())
    {

      // Aktuelles Datum und Zeit setzen, falls die Uhr noch nicht läuft
      RTC.adjust (DateTime (2000, 0, 0, 0, 0, 0));

      Serial.println (F("RTC Error"));
      lcdClearL (1);
      lcd.print F(("RTC Error"));
    }
  else
    {
      lcdClearL (1);
      lcd.print ("RTC l");
      lcd.write (0xE1);
      lcd.print (F("uft"));
    }
  SdFile::dateTimeCallback (dateTime);
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode (SS, OUTPUT);

  // see if the card is present and can be initialized:
  if (!SD.begin (chipSelect))
    {
      lcdClearL (2);
      Serial.println (F("no sd"));
      lcd.print (F("Keine SD-Karte!"));
      // don't do anything more:
      while (!SD.begin (chipSelect))
	{
	  delay (500);
	}
    }
  //lcd.setCursor (0, 2);
  //lcd.print (F("SD Karte erkannt!   "));
  Serial.println (freeMemory ());
  delay (300);
  DateTime now = RTC.now ();
  //SdFile::dateTimeCallback(now);
  char charFileName[13] = "";
  char buf[6] = "";
  itoa (now.day (), buf, 10);
  strcat (charFileName, buf);
  strcat (charFileName, ".");
  itoa (now.month (), buf, 10);
  strcat (charFileName, buf);
  strcat (charFileName, ".");
  itoa ((now.year ()), buf, 10);
  strcat (charFileName, buf);
  strcat (charFileName, ".csv");

  Serial.println (charFileName);
  lcdClearL (2);
  lcd.print (charFileName);
  // Öffnen
  if (!dataFile.open (charFileName, O_RDWR | O_CREAT | O_AT_END))
  //if (!dataFile)
    {
      Serial.println (F("er .csv"));
      lcdClearL (3);
      lcd.print (F("Error .csv Datei"));
      // Wait forever since we cant write data
      while (1)
	;
    }

  sensors.begin ();
  adresseAusgeben (); /* Adresse der Devices ausgeben */

  delay (1000);

  //adresseAusgeben (); /* Adresse der Devices ausgeben */
  lcd.clear ();
  /*
   dataFile.println (F("Datum;Uhrzeit;Sensor1;Sensor2;Sensor3"));
   dataFile.print (F("dd.mm.yyyy;hh.mm.ss;"));
   dataFile.print (0xdf);
   dataFile.println (F("C;Sensor2;Sensor3"));*/
  //Timer zum Loggen auf 0 setzten
  tLoop.restart ();
}

void
loop ()
{
  DateTime now = RTC.now (); // aktuelle Zeit abrufen
  sensors.requestTemperatures (); // Temperatursensor(en) auslesen
  lcd.clear ();
  lcdPrintTime (now);
  lcdPrintTempAdc (ADCPIN);
  for (byte i = 0; i < sensors.getDeviceCount (); i++)
    { // Temperatur ausgeben
      if (i == 0)
	lcd.setCursor (0, 2);
      if (i == 2)
	lcd.setCursor (0, 3);
      lcd.print (temperature (i + 1, sensors.getTempC (sensorsa[i])));
    }

  if (tLoop.t_since_start () > LOGTIME)
    {
      tLoop.restart ();

      // Erzeuge den Datenstring für die csv Datei
      String dataString = "";
      dataString += date_string (now);
      dataString += ';';
      dataString += time_string (now);
      dataString += ';';
      // read the sensors and append to the string:
      dataString += read_temp (ADCPIN);
      dataString += ';';
      //dataString += String (analogRead (ADCPIN));
      //dataString += ';';
      for (byte i = 0; i < sensors.getDeviceCount (); i++)
	{ // Temperatur ausgeben
	  dataString += String (sensors.getTempCByIndex (i));
	  dataString += ';';
	}
      dataFile.println (dataString);

      // print to the serial port too:
      Serial.println (dataString);

      Serial.println (freeMemory ());

      // The following line will 'save' the file to the SD card after every
      // line of data - this will use more power and slow down how much data
      // you can read but it's safer!
      // If you want to speed up the system, remove the call to flush() and it
      // will save the file only every 512 bytes - every time a sector on the
      // SD card is filled with data.
      dataFile.flush ();
      sensors.begin ();
    }
  delay (LCDTIME);
}

static void
lcdClearL (uint8_t line)
{
  lcd.setCursor (0, line);
  lcd.print (F("                    "));
  lcd.setCursor (0, line);
}

//Messe Temp. 0V=0gradC 5V=100gradC

static void
lcdPrintTime (DateTime datetime)
{
  lcd.setCursor (0, 0);
  lcd.print (date_time_string (datetime));
}

static void
lcdPrintTempAdc (uint8_t pin)
{
  lcdClearL (1);
  lcd.print ("T1:");
  lcd.print (read_temp (pin));
  lcd.write (0xdf);
  lcd.print ('C');
}

static String
read_temp (uint8_t pin)
{
  String temp;
  char buffer[8];
  float v = analogRead (pin);
  float t = 100 * v / 1023;
  temp = floatToString (buffer, t, 2, true);
  return temp;
}

// Datums String
static String
date_string (DateTime datetime)
{
  String s = "";
  if (datetime.day () < 10)
    s += '0';
  s += String (datetime.day ());
  s += '.';
  if (datetime.month () < 10)
    s += '0';
  s += String (datetime.month ());
  s += '.';
  s += String (datetime.year ());
  return s;
}

// Uhrzeit String
static String
time_string (DateTime datetime)
{
  String s = "";
  s += String (datetime.hour ());
  s += ':';
  if (datetime.minute () < 10)
    s += '0';
  s += String (datetime.minute ());
  s += ':';
  if (datetime.second () < 10)
    s += '0';
  s += String (datetime.second ());
  return s;
}

//Datums Uhrzeit String;
static String
date_time_string (DateTime datetime)
{
  String s = "";
  s += date_string (datetime);
  s += ' ';
  if (datetime.hour () < 10)
    s += ' ';
  s += time_string (datetime);
  return s;
}

// Temperatur String
static String
temperature (byte num, float temp)
{
  String s = "";
  s += "T";
  s += String (num + 1);
  s += ':';
  s += String (temp);
  s.setCharAt (8, 0xDF);
  s += "C ";
  return s;
}

void
adresseAusgeben (void)
{
  byte i;
  byte present = 0;
  byte data[12];
  byte addr[8];

  Serial.print ("Suche 1-Wire-Devices...\n\r");      // "\n\r" is NewLine
  while (ourWire.search (addr))
    {
      Serial.print ("\n\r\n\r1-Wire-Device gefunden mit Adresse:\n\r");
      for (i = 0; i < 8; i++)
	{
	  Serial.print ("0x");
	  if (addr[i] < 16)
	    {
	      Serial.print ('0');
	    }
	  Serial.print (addr[i], HEX);
	  if (i < 7)
	    {
	      Serial.print (", ");
	    }
	}
      if (OneWire::crc8 (addr, 7) != addr[7])
	{
	  Serial.print ("CRC is not valid!\n\r");
	  return;
	}
    }
  Serial.println ();
  ourWire.reset_search ();
  return;
}
