/*
 ----------------
 Taqumas Cavair 1.23 - Kellerlüftung taupunktgesteuert.
 ----------------
 
 
 April bis Juli 2013
 von taquma
 fuer Fabs&Family&jede/n
 
 
 Gemacht aus dem Hello World-Beispiel der LiquidCrystal Library
 http://www.arduino.cc/en/Tutorial/LiquidCrystal
 
 und dem
 Example testing sketch for various DHT humidity/temperature sensors
 Written by ladyada, public domain
 
 und der Taupunktberechnung von 
 http://www.wettermail.de/wetter/feuchte.html#f4
 
 und der Funksteckdosensteuerung von
 http://blog.sui.li/2011/04/12/low-cost-funksteckdosen-arduino/
 
 und mehreren Beispielen aus dem "Arduino-Kochbuch" von Michael Margolis.
 
 This code is in the public domain.
 Copy me, I want to travel...
 
 
 Dieses Programm liest die Werte von 2 Temperatur/Feuchtesensoren DHT22 (wenn vorhanden, 
 andernfalls gibts eine Fehlermeldung), berechnet die Taupunkte und ihre Differenz, und gibt alle Werte auf einem 16x2LCD aus.
 
 Ein Lüfter wird per Funksteckdose taupunkt- und intervallabhängig gesteuert.
 Ziel ist die permanente Entfeuchtung der Kellerluft, indem nur gelüftet wird, falls die Außenluft absolut trockener
 ist, als die Kellerluft. Dafür werden Außen- und Kellertaupunkt verglichen, die über Temperatur- und Feuchtewerte
 errechnet werden. Bei positiver Differenz wird dann der Luefter für 10 Minuten pro Stunde eingeschaltet.
   
 Über einen Poti können 7 verschiedene Anzeigemodi eingestellt werden:
 Modus 1: Anzeige aus
 Modus 2: Feuchte und Temperatur Aussen und Innen
 Modus 3: Taupunkte Aussen und Innen
 Modus 4: Taupunkte-Differenz (Innen minus Aussen) und OnTime
 Modus 5: Luefterstatus und Zykluszeit
 Modus 6: Zählerstand
 Modus 7: alle Werte nacheinander
 
 To do: 
 - Ethernet (Siehe CavAir2! (coming soon!))

 Revs:
 - 1.0  volle Funktion, Mai 2013
 - 1.1  + verschiedene Anzeigemodi
 - 1.22 + Zähler für Lüfter, 26.6.2013
 - 1.23 + Überlauf durch "unsigned long Zyklus" erst nach 50 Tagen. Vorher 25 Tage, da nur "long Zyklus". 07.07.2013
        + Kellermindesttemperatur (eingestellt auf 12°C)
 
 no(c)tq 2013
 -------------------------------------------------------------------------------------------------------------- 
 
 ----------------
 Taqumas Cavair 1.23
 ----------------
 */

#include <LiquidCrystal.h> // LCD-library importieren
#include "DHT.h" // DHT-library importieren
#include <RCSwitch.h> // Funksteckdosen-Library importieren
// Pinbelegung
// not #define LCD 2, 3, 4, 5
#define LUEFTERPIN 6 // Lüfter an Pin 6
RCSwitch Luefter = RCSwitch();  // Klassendefinition "Luefter" für die RCSwitch-Library
#define DHTPINaussen 8     // DHT fuer Außen an Pin 8
DHT dhtaussen(DHTPINaussen, DHT22); // Definition des DHT-Objekts
#define DHTPINkeller 9     // DHT fuer den Keller an Pin 9
DHT dhtkeller(DHTPINkeller, DHT22); // Definition des DHT-Objekts
#define POTI 14 // DrehPoti an Pin 14 (A0)

// Sensorpinbelegung:
// Connect pin 1 (on the left) of the sensor to +5V
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
#define eineSekunde 1000 // 1 Sekunde = 1000 Millisekunden
#define eineMinute 60000 // eine Minute = 60000 Millisekunden
#define zehnMinuten 600000 // usw.
#define eineStunde 3600000 // usf.
#define Hysterese 6.0 // 

unsigned long Zyklus;
boolean LuefterStatus;
long Anzeit = zehnMinuten; 
long Auszeit = eineStunde; 
unsigned int Zaehler = 0; // zaehlt, wie oft der Luefter an war
byte KellerMinTemp = 12; // evtl auf float ändern...

void setup() {
  Serial.begin(9600);
  pinMode(POTI, INPUT);
  Luefter.enableTransmit(LUEFTERPIN); // Funksender an Pin 6
  Luefter.setPulseLength(500); // Pulslänge zum Beschalten der Funksteckdose 500 ms

  lcd.begin(16, 2);
  lcd.print("*** TAQUMAS  ***"); 
  lcd.setCursor(0, 1); 
  lcd.print("* CAVAIR 1.23  *");
  delay(5000);
  
  lcd.setCursor(0, 0); 
  lcd.print(" 32K RAM SYSTEM "); 
  lcd.setCursor(0, 1); 
  lcd.print("16888 BYTES FREE");
  delay(3000);
  
  dhtaussen.begin(); // Öffne die Datenkanäle der Sensoren
  dhtkeller.begin();
  Luefter.switchOn(2, 1); // Lüfter (Steckdose 1 auf Kanal 2) zum Initialisieren einschalten
  delay(1000);
  LuefterStatus = true; Zaehler +=1;
  Zyklus = millis(); // Startzeit speichern
}

void loop() { 
   // Sensoren auslesen
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float Aussenfeuchte = dhtaussen.readHumidity();
  float Aussentemperatur = dhtaussen.readTemperature();
  float Kellerfeuchte = dhtkeller.readHumidity();
  float Kellertemperatur = dhtkeller.readTemperature();
 
   // Taupunkteberechnung:
   // Parameter:
  const float a = 7.5;
  const float b = 237.3; // für T >= 0
  // a = 7.6, b = 240.7 für T < 0 über Wasser (Taupunkt)
  // a = 9.5, b = 265.5 für T < 0 über Eis (Frostpunkt)

  // Aussen
  float ra = Aussenfeuchte;
  float Ta = Aussentemperatur;
  float SDDa = 6.1078 * pow(10,(a*Ta)/(b+Ta)); // Saettigungsdampfdruck in hP
  float DDa = (ra/100 * SDDa);  // Dampfdruck in hP
  float va = log10(DDa/6.1078); // Bezeichnung fuer v unbekannt
  float TDa = (b*va/(a-va));  // Taupunkttemperatur in Grad C

  // Keller
  float rk = Kellerfeuchte;
  float Tk = Kellertemperatur;
  float SDDk = 6.1078 * pow(10,(a*Tk)/(b+Tk)); // Saettigungsdampfdruck in hP
  float DDk = (rk/100 * SDDk);  // Dampfdruck in hP
  float vk = log10(DDk/6.1078); // Bezeichnung fuer v unbekannt
  float TDk = (b*vk/(a-vk));  // Taupunkttemperatur in Grad C
  
  float DeltaTp = TDk - TDa; // DeltaTP ist die Differenz der Taupunkte Keller zu Aussen und NICHT die Taupunktdifferenz!
  // Die Taupunktdifferenz ist Temperatur - Taupunkt, also T-TD, d.h. die Temperaturspanne, um die die Temperatur
  // absinken muss, um die Feuchte zu kondensieren. Wir haben 2 Taupunkte: Aussen und Keller.
  // DeltaTP muß positiv sein, um den Keller zu entfeuchten, d.h den Lüfter einzuschalten.

 // Steuerlogik
  if ((LuefterStatus == false ) && ((millis() - Zyklus) > Auszeit) && (DeltaTp > Hysterese)  && (Aussentemperatur > 0.0) && (Kellertemperatur > KellerMinTemp) ){   // Aussentemp > 0 evtl. ausklammern. 
      Luefter.switchOn(2, 1);    
      LuefterStatus = true; Zaehler +=1;
      delay(1000);     
      Zyklus = millis();
   }
 else if ((LuefterStatus == true) && ((millis() - Zyklus) > Anzeit)){
      Luefter.switchOff(2, 1);    
      LuefterStatus = false; 
      delay(1000);     
   } 
   
 // Datenausgabe
  int potiwert = map(analogRead(POTI), 0, 1023, 1, 7); // Poti für Anzeigemodus auslesen (7 Anzeigemodi)
   // check if returns are valid, if they are NaN (not a number) then something went wrong!
  if (isnan(Aussentemperatur) || isnan(Aussenfeuchte)  || isnan(Kellertemperatur)  || isnan(Kellerfeuchte)  ) 
  {
    lcd.setCursor(0, 0);
    lcd.println("DHT read error!   "); 
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.noDisplay(); 
    delay(300); 
    lcd.display();
    delay(500);
  } 
  else {
    switch(potiwert)
    {
    case 1: // Anzeige aus
      lcd.noDisplay();
      break;
      
    case 2: // Feuchte und Temperatur Aussen und Innen anzeigen
      lcd.display();
      lcd.setCursor(0, 0); // Cursur in erste Zeile, erstes Zeichen
      lcd.print("A: "); 
      lcd.print(Aussenfeuchte); 
      lcd.setCursor(7, 0); 
      lcd.print("% "); //  
      lcd.print(Aussentemperatur); 
      lcd.setCursor(13, 0); 
      lcd.write(B11011111); 
      lcd.println("C.");
      lcd.setCursor(0, 1); // Cursur in zweite Zeile, erstes Zeichen
      lcd.print("K: "); 
      lcd.print(Kellerfeuchte); 
      lcd.setCursor(7, 1); 
      lcd.print("% ");  
      lcd.print(Kellertemperatur); 
      lcd.setCursor(13, 1); 
      lcd.write(B11011111); 
      lcd.println("C.");
      break;
      
    case 3: // Taupunkte Aussen und Innen anzeigen
      lcd.display();
      lcd.setCursor(0, 0); //
      lcd.print("TP(A):  "); 
      lcd.print(TDa);  
      lcd.write(B11011111); 
      lcd.println("C. "); // TDa = Taupunkt Aussen
      lcd.setCursor(0, 1); // 
      lcd.print("TP(K):  "); 
      lcd.print(TDk);  
      lcd.write(B11011111); 
      lcd.println("C. "); // TDk = Taupunkt Keller
      break;
      
    case 4: // Taupunkte-Differenz und OnTime anzeigen
      lcd.display();
      lcd.setCursor(0, 0); // 
      lcd.print("DTp: "); 
      lcd.print(DeltaTp);  
      lcd.write(B11011111); 
      lcd.println("C.    "); // DTp = Differenz der Taupunkte
      lcd.setCursor(0, 1); // 
      lcd.print("OnTime: "); 
      lcd.print(millis()/eineStunde); 
      lcd.println(" Std.  ");
      break;
      
    case 5: // Luefterstatus und Zyklus anzeigen
      lcd.display();
      lcd.setCursor(0, 0); // 
      lcd.print("Luefterstatus: "); 
      lcd.print(LuefterStatus);
      lcd.setCursor(0, 1); // 
      lcd.print("Zyklus: ");
      lcd.print((millis()-Zyklus)/eineMinute); 
      lcd.println(" Min.     "); // 
      break;
      
      case 6: // Zaehlerstand anzeigen
      lcd.display();
      lcd.setCursor(0, 0); // 
      lcd.print("Zaehler: "); 
      lcd.print(Zaehler);
      lcd.print("      ");
      lcd.setCursor(0, 1); // 
      lcd.print("                ");
      break;   
      
    case 7: // alle Werte hintereinander weg, jeweils 2 Sekunden
      lcd.display();
      lcd.display();
      lcd.setCursor(0, 0); // Cursur in erste Zeile, erstes Zeichen
      lcd.print("A: "); 
      lcd.print(Aussenfeuchte); 
      lcd.setCursor(7, 0); 
      lcd.print("% "); //  
      lcd.print(Aussentemperatur); 
      lcd.setCursor(13, 0); 
      lcd.write(B11011111); 
      lcd.println("C.");
      lcd.setCursor(0, 1); // Cursur in zweite Zeile, erstes Zeichen
      lcd.print("K: "); 
      lcd.print(Kellerfeuchte); 
      lcd.setCursor(7, 1); 
      lcd.print("% ");  
      lcd.print(Kellertemperatur); 
      lcd.setCursor(13, 1); 
      lcd.write(B11011111); 
      lcd.println("C.");
      delay(3000);
      lcd.display();
      lcd.setCursor(0, 0); //
      lcd.print("TP(A):  "); 
      lcd.print(TDa);  
      lcd.write(B11011111); 
      lcd.println("C. "); // TDa = Taupunkt Aussen
      lcd.setCursor(0, 1); // 
      lcd.print("TP(K):  "); 
      lcd.print(TDk);  
      lcd.write(B11011111); 
      lcd.println("C. "); // TDk = Taupunkt Keller
      delay(3000);
      lcd.display();
      lcd.setCursor(0, 0); // 
      lcd.print("DTp: "); 
      lcd.print(DeltaTp);  
      lcd.write(B11011111); 
      lcd.println("C.    "); // DTp = Differenz der Taupunkte
      lcd.display();
      lcd.setCursor(0, 1); // 
      lcd.print("OnTime: "); 
      lcd.print(millis() / eineStunde); 
      lcd.println(" Std.  ");
      delay(3000);  
      lcd.display();
      lcd.setCursor(0, 0); // 
      lcd.print("Luefterstatus: "); 
      lcd.print(LuefterStatus);
      lcd.setCursor(0, 1); // 
      lcd.print("Zyklus: ");
      lcd.print((millis() - Zyklus) / eineMinute);
      lcd.println(" Min.  ");
      delay(3000);     
      lcd.display();
      lcd.setCursor(0, 0); // 
      lcd.print("Zaehler: "); 
      lcd.print(Zaehler);
      lcd.print("      ");
      lcd.setCursor(0, 1); // 
      lcd.print("                ");
      delay(2000);
      break;
    }
  } 
}
 // ... und wieder von vorn. (hoch zu void loop(){


/* 
 
 Taupunktberechnung:
 -------------------
 
 
 Bezeichnungen:
 r = relative Luftfeuchte
 T = Temperatur in °C
 TK = Temperatur in Kelvin (TK = T + 273.15)
 TD = Taupunkttemperatur in °C
 DD = Dampfdruck in hPa
 SDD = Sättigungsdampfdruck in hPa
 
 Parameter:
 a = 7.5, b = 237.3 für T >= 0
 a = 7.6, b = 240.7 für T < 0 über Wasser (Taupunkt)
 a = 9.5, b = 265.5 für T < 0 über Eis (Frostpunkt)
 
 R* = 8314.3 J/(kmol*K) (universelle Gaskonstante)
 mw = 18.016 kg (Molekulargewicht des Wasserdampfes)
 AF = absolute Feuchte in g Wasserdampf pro m3 Luft
 
 Formeln:
 
 SDD(T) = 6.1078 * 10^((a*T)/(b+T))
 DD(r,T) = r/100 * SDD(T)
 r(T,TD) = 100 * SDD(TD) / SDD(T)
 TD(r,T) = b*v/(a-v) mit v(r,T) = log10(DD(r,T)/6.1078)
 AF(r,TK) = 10^5 * mw/R* * DD(r,T)/TK; AF(TD,TK) = 10^5 * mw/R* * SDD(TD)/TK 
 
 
 Taupunkttabelle
 ---------------
 
 
               relative Luftfeuchte in %
 Temperatur
 Raumluft°C 
 	
        30% 	35% 	40% 	45% 	50% 	55% 	60% 	65% 	70% 	75% 	80% 	85% 	90% 	95%
 30 	10,5 	12,9 	14,9 	16,8 	18,4 	20,0 	21,4 	22,7 	23,9 	25,1 	26,2 	27,2 	28,2 	29,1
 29 	9,7 	12,0 	14,0 	15,9 	17,5 	19,0 	20,4 	21,7 	23,0 	24,1 	25,2 	26,2 	27,2 	28,1
 28 	8,8 	11,1 	13,1 	15,0 	16,6 	18,1 	19,5 	20,8 	22,0 	23,2 	24,2 	25,2 	26,2 	27,1
 27 	8,0 	10,2 	12,2 	14,1 	15,7 	17,2 	18,6 	19,9 	21,1 	22,2 	23,3 	24,3 	25,2 	26,1
 26 	7,1 	9,4 	11,4 	13,2 	14,8 	16,3 	17,6 	18,9 	201 	21,2 	22,3 	23,3 	24,2 	25,1
 25 	6,2 	8,5 	10,5 	12,2 	13,9 	15,3 	16,7 	18,0 	19,1 	20,3 	21,3 	22,3 	23,2 	24,1
 24 	5,4 	7,6 	9,6 	11,3 	12,9 	14,4 	15,8 	17,0 	18,2 	19,3 	20,3 	21,3 	22,3 	23,1
 23 	4,5 	6,7 	8,7 	10,4 	12,0 	13,5 	14,8 	16,1 	17,2 	18,3 	19,4 	20,3 	21,3 	22,2
 22 	3,6 	5,9 	7,8 	9,5 	11,1 	12,5 	13,9 	15,1 	16,3 	17,4 	18,4 	19,4 	20,3 	21,2
 21 	2,8 	5,0 	6,9 	8,6 	10,2 	116 	12,9 	14,2 	15,3 	16,4 	17,4 	18,4 	19,3 	20,2
 20 	1,9 	4,1 	6,0 	7,7 	9,3 	10,7 	12,0 	13,2 	14,4 	15,4 	16,4 	17,4 	18,3 	19,2
 19 	1,0 	3,2 	5,1 	6,8 	8,3 	9,8 	11,1 	12,3 	13,4 	14,5 	15,5 	16,4 	17,3 	18,2
 18 	0,2 	2,3 	4,2 	5,9 	7,4 	8,8 	10,1 	11,3 	12,5 	13,5 	14,5 	16,4 	16,3 	17,2
 17 	-0,6 	1,4 	3,3 	5,0 	6,5 	7,9 	9,2 	10,4 	11,5 	12,5 	13,5 	15,5 	15,3 	16,2
 16 	-1,4 	-0,5 	2,4 	4,1 	5,6 	7,0 	8,2 	9,4 	10,5 	11,6 	12,6 	14,5 	14,4 	15,2
 15 	-2,2 	-0,3 	1,5 	3,2 	4,7 	6,1 	7,3 	8,5 	9,6 	10,6 	11,6 	13,5 	13,4 	14,2
 14 	-2,9 	-1,0 	0,6 	2,3 	3,7 	5,1 	6,4 	7,5 	8,6 	9,6 	10,6 	12,5 	12,4 	13,2
 13 	-3,7 	-1,9 	0,1 	1,3 	2,8 	4,2 	5,5 	6,6 	7,7 	8,7 	9,6 	10,5 	11,4 	12,2
 12 	-4,5 	-2,6 	1,0 	0,4 	1,9 	3,2 	4,5 	5,7 	6,7 	7,7 	8,7 	9,6 	10,4 	11,2
 11 	-5,2 	-3,4 	1,8 	-0,4 	1,0 	2,3 	3,5 	4,7 	5,8 	6,7 	7,7 	8,6 	9,4 	10,2
 10 	-6,0 	-4,2 	2,6 	-1,2 	0,1 	1,4 	2,6 	3,7 	4,8 	5,8 	6,7 	7,6 	8,4 	9,2
 
 */
