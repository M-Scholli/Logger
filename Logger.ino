/*
 FVA Datenlogger
 */
#include <avr/pgmspace.h>
#include <SPI.h>
#include <Sdfat.h>
#include <Wire.h>
#include "RTClib.h"
#include "OneWire.h"
#include "DallasTemperature.h"
#include <LiquidCrystal.h>
#include <MemoryFree.h>
#include "Timer.h"
#include "SdFatUtil.h"
#include "Button.h"
#include "Alarm.h"

#define LOGTIME 	10000 	//Zeit zwischen den Messungen in ms;
#define LCDTIME 	500	//Wiederholungszeit in ms zum aktualiseieren der Werte im LCD
#define ADCPIN		A7	// Pin an dem der Temperatursensor 1 (0-10V) Adc angeschlossen ist
#define SENSOR_NUM	3 	// Max. Anzahl Sensoren vom Typ DS18X20
#define CS_SD		10
#define ADC_REG		A6

// Initalisierung des LCDs mit den verwendeten Pins
LiquidCrystal lcd (2, 3, 4, 5, 6, 7, 8);

Button button (17, 50, 800);

Alarm alarm (&lcd);

//----------------------------------------
//DS18x20
//----------------------------------------

#define ONE_WIRE_BUS A1

OneWire ourWire (ONE_WIRE_BUS);

DallasTemperature sensors (&ourWire);

//DeviceAdressen der einzelnen DS18x20 Temperatursensoren
DeviceAddress sensorenDs1820[SENSOR_NUM] =
  {
    { 0x28, 0xBA, 0xFA, 0x1C, 0x7, 0x0, 0x0, 0x9D },
    { 0x28, 0xC9, 0xFA, 0xF8, 0x4, 0x0, 0x0, 0xFF },
    { 0x28, 0x58, 0xE4, 0xF8, 0x4, 0x0, 0x0, 0xA7 }, };

float temperaturen[4] =
  { 0, 0, 0, 0 };

//-----------------------------------------
//RTC-MODUL
//-----------------------------------------

RTC_DS1307 RTC;

// nötig für SDFat für ein Erstellungsdatum des Sd Files
void
dateTime (uint16_t* date, uint16_t* time)
{
  DateTime now = RTC.now ();

  // return date using FAT_DATE macro to format fields
  *date = FAT_DATE (now.year (), now.month (), now.day ());

  // return time using FAT_TIME macro to format fields
  *time = FAT_TIME (now.hour (), now.minute (), now.second ());
}

//Format für Datum Uhrzeit in der CSV Datei
ostream&
operator << (ostream& os, DateTime& dt)
{
  os << dt.year () << '/' << int (dt.month ()) << '/' << int (dt.day ()) << ';';
  os << int (dt.hour ()) << ':' << setfill ('0') << setw (2)
      << int (dt.minute ());
  os << ':' << setw (2) << int (dt.second ()) << setfill (' ');
  return os;
}

// Timer für das Aufzeichnungsintervall
Timer tLoop;
Timer tlcd;

//-----------------------------------------
// SD-KARTEN MODUL
//-----------------------------------------

SdFat SD;
ofstream dataFile;
char buf[55];

//-----------------------------------------------------------------------------------------
// Initialisierungen
//-----------------------------------------------------------------------------------------

static void
rtcInit (void)
{
  // Initialisiere RTC
  RTC.begin ();
  if (!RTC.isrunning ())
    {
      // Aktuelles Datum und Zeit setzen, falls die Uhr noch nicht läuft:
      RTC.adjust (DateTime (2000, 0, 0, 1, 0, 1));
      lcdClearLine (1);
      lcd.print F(("RTC Error"));
    }
  else
    {
      lcdClearLine (1);
      lcd.print ("RTC l");
      lcd.write (0xE1);
      lcd.print (F("uft"));
    }
}

static void
sdInit (void)
{
  SdFile::dateTimeCallback (dateTime);
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode (SS, OUTPUT);

  // see if the card is present and can be initialized:
  if (!SD.begin (CS_SD))
    {
      lcdClearLine (2);
      Serial.println (F("no sd"));
      lcd.print (F("Keine SD-Karte!"));
      // don't do anything more:
      while (!SD.begin (CS_SD))
	{
	  delay (1000);
	}
    }
}

static void
sdOpenFile (void)
{
  DateTime now = RTC.now ();
  char charFileName[13] = "";
  char bufer[6] = "";
  itoa (now.day (), bufer, 10);
  strcat (charFileName, bufer);
  strcat (charFileName, ".");
  itoa (now.month (), bufer, 10);
  strcat (charFileName, bufer);
  strcat (charFileName, ".");
  itoa ((now.year ()), bufer, 10);
  strcat (charFileName, bufer);
  strcat (charFileName, ".csv");
  Serial.println (charFileName);
  lcdClearLine (2);
  lcd.print (charFileName);
  dataFile.open (charFileName, ios::out | ios::app); // Erstellt eine neue Datei falls keine zum öffnen vorhanden ist

  if (!dataFile.is_open ())
    {
      lcdClearLine (3);
      lcd.print (F("Error .csv Datei"));
      while (1)
	;
    }
}

//-----------------------------------------------------------------------------------------
// Ausgaben für LCD oder Serial
//-----------------------------------------------------------------------------------------

//Zeile löschen im LCD
static void
lcdClearLine (uint8_t line)
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
  lcd.print (DateAndTimeString (datetime));
}

static float
readTempFloat (uint8_t pin)
{
  float t = analogRead (pin);
  t = 100 * t / 1023;
  return t;
}

// Datums String
static String
DateString (DateTime datetime)
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
TimeString (DateTime datetime)
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
DateAndTimeString (DateTime datetime)
{
  String s = "";
  s += DateString (datetime);
  s += ' ';
  if (datetime.hour () < 10)
    s += ' ';
  s += TimeString (datetime);
  return s;
}

static String
sensorNum (uint8_t i)
{
  String s = "";
  s += "T";
  s += String (i);
  return s;
}

// Temperatur String
static String
TemperaturString (byte num, float temp)
{
  String s = "";
  s += sensorNum (num + 1);
  s += ':';
  if (temp == -127)
    {
      s += F("No.Co.");
    }
  else
    {
      if (abs(temp) < 10)
	s += ' ';
      s += String (temp);
      s.setCharAt (7, 0xDF);
      s += "C ";
    }
  return s;
}

void
TempsAuslesen (void)
{
  temperaturen[0] = readTempFloat (ADCPIN);
  for (uint8_t i = 1; i < 4; i++)
    {
      temperaturen[i] = sensors.getTempC (sensorenDs1820[i - 1]);
    }
}

void
AlarmMenue (void)
{
  uint8_t j = 0;
  lcd.clear ();
  lcd.setCursor (0, 0);
  lcd.print (F("Alarm Einstellungen"));
  lcd.setCursor (0, 1);
  lcd.print (F("Min:"));
  lcd.setCursor (4, 1);
  lcd.print (F("Max: aktiv.Sen.:"));
  alarm.lcdWerte ();
  alarm.lcdCursorAn (j);
  delay (1000);
  while (j < 6)
    {
      button.check_button_state ();
      int adc = analogRead (ADC_REG);
      if (j < 2)
	{
	  alarm.Werte[j] = adc / 10.24;
	}
      else
	{
	  if (adc > 600)
	    alarm.Werte[j] = 1;
	  else if (adc < 400)
	    alarm.Werte[j] = 0;
	}
      alarm.lcdWert (j);
      if (button.button_pressed_short ())
	{
	  alarm.lcdCursorAus (j);
	  j++;
	  if (j < 6)
	    {
	      alarm.lcdCursorAn (j);
	    }
	}
      delay (100);
    }
  lcd.clear ();
}

void
lcdAlarm (void)
{
  String smax = "";
  String smin = "";
  uint8_t aktiv = 0;
  uint8_t aktMin = 0;
  uint8_t aktMax = 0;
  for (uint8_t i = 0; i < 4; i++)
    {
      if (alarm.alarm[i] == 1)
	{
	smax += sensorNum (i + 1);
	smax += ' ';
	}
    }
  if (smax.length() > 1)
    {
      smax.trim();
      smax += ">Max ";
      aktMax = 1;
    }
  for (uint8_t j = 0; j < 4; j++)
    {
      if (alarm.alarm[j] == 2)
	{
	smin += sensorNum (j + 1);
	smin += ' ';
	}
    }
  if (smin.length() > 1)
    {
      smin.trim();
      smin +="<Min";
      aktMin = 1;
    }
  lcdClearLine(1);
  if (aktMax == 1)
    {
      lcd.print(smax);
      aktiv = 1;
    }
  if (aktMin == 1)
    {
      lcd.print(smin);
      aktiv = 1;
    }
}

void
LcdTempAnzeige (void)
{
  DateTime now = RTC.now (); // aktuelle Zeit abrufen
  sensors.requestTemperatures (); // Temperatursensor(en) auslesen (überflüssig?) toDo --> Dauerabfrage der Sensoren
  lcdPrintTime (now);
  //Temperaturen ausgeben:
  for (byte i = 0; i < 4; i++)
    {
      switch (i)
	{
	case 0:
	  lcd.setCursor (0, 2);
	  break;
	case 1:
	  lcd.setCursor (10, 2);
	  break;
	case 2:
	  lcd.setCursor (0, 3);
	  break;
	case 3:
	  lcd.setCursor (10, 3);
	  break;
	}
      lcd.print (TemperaturString (i, temperaturen[i]));
    }
}

// Schreibt die gesammelten Daten auf die Sd Karte
// todo mit interupt timer aufrufen.
void
logSdKarte (void)
{
  DateTime now = RTC.now (); // aktuelle Zeit abrufen
  obufstream bout (buf, sizeof(buf));
  tLoop.restart ();
  bout << now;
  bout << ';';
  for (byte i = 0; i < 4; i++)
    {
      float temp = temperaturen[i];
      if (temp != -127)
	{
	  bout << temp;
	}
      bout << ';';
    }
  bout << endl;		//Zeilen Sprung am Ende der Zeile
  dataFile << buf << flush;	// Buffer auf die SD Karte schreiben
  Serial.print (buf);
  // Überprüfung ob das Schreiben erfolgreich war, bzw. die SD Karte noch vorhanden ist.
  if (!dataFile)
    {
      lcd.clear ();
      lcdPrintTime (now);
      lcd.setCursor (0, 1);
      lcd.print ("SD-Error");
      dataFile.close ();
      sdInit ();
      sdOpenFile ();
    }
}

// Kann verwendet werden um die Adressen der DS18x20 Sensoren heraus zu finden
/*
 void
 adresseAusgeben (void)
 {
 byte i;
 byte addr[8];

 Serial.println (F("1W. Adr.:"));      // "\n\r" is NewLine
 while (ourWire.search (addr))
 {
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
 Serial.println ();
 if (OneWire::crc8 (addr, 7) != addr[7])
 {
 Serial.print (F("CRCinvalid!\n\r"));
 return;
 }
 }
 Serial.println ();
 //ourWire.reset_search ();
 return;
 }
 */

//-----------------------------------------------------------------------------------------
// Main Setup
//-----------------------------------------------------------------------------------------//------------------------------------------------------------------------------
void
setup ()
{
  // Initialisiere I2C
  Wire.begin ();
  // Initialisiere SerialUSB Interface
  Serial.begin (9600);
  lcd.begin (20, 4);
  lcd.clear ();
  lcd.print (F("FVA Logger 1.0"));
  lcd.setCursor (0, 1);
  lcd.print (F("Starte RTC"));
  lcd.setCursor (0, 2);
  lcd.print (F("Init. SD Karte"));
  delay (1000);
  rtcInit ();
  sdInit ();
  Serial.println (freeMemory ());
  delay (300);
  sdOpenFile ();
  //adresseAusgeben ();
  delay (1000);
  lcd.clear ();
  obufstream bout (buf, sizeof(buf));
  bout << pstr("Datum;Uhrzeit;T1;T2;T3;T4;T5");
  dataFile << buf << endl;
}

//-----------------------------------------------------------------------------------------
// Main Loop
//-----------------------------------------------------------------------------------------
void
loop ()
{
  button.check_button_state ();
  //Aufruf des Alarm Menüs
  if (button.button_press_long ())
    {
      AlarmMenue ();
    }
  //Aufruf des Lcd Aktualisierung
  if (tlcd.t_since_start () > LCDTIME)
    {
      TempsAuslesen ();
      LcdTempAnzeige ();
      alarm.checkAlarm (temperaturen);
      lcdAlarm();
    }
  //Aufruf der Aufzeichnung auf der SD-Karte
  if (tLoop.t_since_start () > LOGTIME)
    {
      TempsAuslesen ();
      logSdKarte ();
      Serial.println (freeMemory ());
    }
}
