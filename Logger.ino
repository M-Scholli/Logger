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
#include <SD.h>
#include <Wire.h>    // I2C-Bibliothek einbinden
#include "RTClib.h"  // RTC-Bibliothek einbinden
#include "floatToString.h"
//#include "OneWire.h"
//#include "DallasTemperature.h"
#include <LiquidCrystal.h>
#include <MemoryFree.h>


// initialize the library with the numbers of the interface pins
LiquidCrystal lcd (2, 3, 4, 5, 6, 7, 8);

//#define ONE_WIRE_BUS 9

//OneWire ourWire (ONE_WIRE_BUS); /* Ini oneWire instance */

//DallasTemperature sensors (&ourWire);/* Dallas Temperature Library für Nutzung der oneWire Library vorbereiten */

RTC_DS1307 RTC;      // RTC Modul

// On the Ethernet Shield, CS is pin 4. Note that even if it's not
// used as the CS pin, the hardware CS pin (10 on most Arduino boards,
// 53 on the Mega) must be left as an output or the SD library
// functions will not work.
const int chipSelect = 10;

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

  lcd.begin (20, 4);

  // Begrüßungstext auf seriellem Monitor und Display ausgeben
  Serial.println (F("FVA Datenlogger"));
  lcd.clear ();
  lcd.print (F("FVA Datenlogger"));
  Serial.println(freeMemory());
  delay (300);
  Serial.println (F("Starte Datum und Zeit"));
  lcd.setCursor (0, 1);
  lcd.print (F("Starte RTC"));
  Serial.println (F("Initializing SD card..."));
  lcd.setCursor (0, 2);
  lcd.print (F("Init. SD Karte"));
  delay (1000);

  if (!RTC.isrunning ())
    {

      // Aktuelles Datum und Zeit setzen, falls die Uhr noch nicht läuft
      RTC.adjust (DateTime (2000, 0, 0, 0, 0, 0));

      Serial.println (F(
	  "Echtzeituhr wurde gestartet und auf Systemzeit gesetzt."));
      lcd.setCursor (0, 1);
      lcd.print F(("RTC Error          "));
    }
  else
    {
      Serial.println (F("Echtzeituhr laeuft bereits."));
      lcd.setCursor (0, 1);
      lcd.print (F("RTC l"));
      lcd.write (0xE1);
      lcd.print (F("uft.          "));
    }

  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode (SS, OUTPUT);

  // see if the card is present and can be initialized:
  if (!SD.begin (chipSelect))
    {
      lcd.setCursor (0, 2);
      Serial.println (F("Keine SD-Karte"));
      lcd.print (F("Keine SD-Karte!     "));
      // don't do anything more:
      while (!SD.begin (chipSelect))
	{
	  delay (500);
	}
    }
  Serial.println (F("SD Karte erkannt!"));
  lcd.setCursor (0, 2);
  lcd.print (F("SD Karte erkannt!   "));
  Serial.println(freeMemory());
  delay (300);
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
    logName += F("EE");
  logName += F(".csv");
  char charFileName[logName.length () + 1];
  logName.toCharArray (charFileName, sizeof(charFileName));
  Serial.println (charFileName);
  lcd.setCursor (0, 3);
  lcd.print (charFileName);
  // Öffnen
  dataFile = SD.open (charFileName, FILE_WRITE);
  if (!dataFile)
    {
      Serial.println (F("error opening .csv"));
      lcd.setCursor (0, 3);
      lcd.print (F("Error .csv Datei    "));
      // Wait forever since we cant write data
      while (1)
	;
    }

  //sensors.begin ();

  delay (1000);

  //adresseAusgeben (); /* Adresse der Devices ausgeben */

  dataFile.println (F("Datum;Uhrzeit;Sensor1;Sensor2;Sensor3"));
  dataFile.println (F("dd.mm.yyyy;hh.mm.ss;°C;Sensor2;Sensor3"));
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
  dataString += String (analogRead (A7));
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
  Serial.println(freeMemory());
  /*
   sensors.requestTemperatures (); // Temperatursensor(en) auslesen

   for (byte i = 0; i < sensors.getDeviceCount (); i++)
   { // Temperatur ausgeben

   show_temperature (i + 1, sensors.getTempCByIndex (i));
   }
   */
  // Take 1 measurement every 500 milliseconds
  delay (6000);
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

/*
 void
 adresseAusgeben (void)
 {
 byte i;
 //byte present = 0;
 //byte data[12];
 byte addr[8];

 Serial.print ("Suche 1-Wire-Devices...");  // "\n\r" is NewLine
 while (ourWire.search (addr))
 {
 Serial.print ("\n\r1-Wire-Device gefunden mit Adresse:\n\r");
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

 // Temperatur ausgeben
 void
 show_temperature (byte num, float temp)
 {

 Serial.print ("Sensor ");
 Serial.print (num);
 Serial.print (": ");
 Serial.print (temp);
 Serial.print (" ");  // Hier müssen wir ein wenig tricksen
 Serial.write (176);  // um das °-Zeichen korrekt darzustellen
 Serial.println ("C");
 }
 */
