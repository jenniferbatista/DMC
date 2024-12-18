// Hochschule Mannheim
// Institut f?r Embedded Systems
// PIC3_Ultraschall_ccp: Entfernungsmessung per Ultraschall + Echtzeituhr
// 13.12.2023 (Poh) sprintf / char LCDtext1+2

#include <stdio.h>
#include <stdlib.h>

#pragma config OSC = HS, WDT = OFF, LVP = OFF, CCP2MUX = OFF // HS Oszillator, Watchdog Timer disabled, Low Voltage Programming

#define Simulator // zum Gebrauch mit Hardware auskommentieren
// Define f?r LCD des neuen, gr?nen Demo-Boards:
// #define NEUE_PLATINE  // Achtung: define vor include! Bei altem braunem Demo-Board auskommentieren!

#include "p18f452.h"
#include "lcd.h"

//   LCD-Strings: "1234567890123456"   // 16 Zeichen/Zeile
char LCDtext1[20] = " Interrupt      "; // LCD Zeile 1: Abstand Ultraschallsensor
char LCDtext2[20] = " Ultraschall    "; // LCD Zeile 2: Echtzeituhr

unsigned int Abstand = 0; // Abstand des Objekts

unsigned char Vorzaehler = 0; // Uhrenvariablen:
unsigned char Stunde = 0;
unsigned char Minute = 0;
unsigned char Sekunde = 0;

unsigned int caputure_werte = 0;
unsigned int i = 0;

//100000 counts (F4240) -> FFFF - F424 = 3035
//Teilerfaktor: 1MHz * 100ms = 1000000Hz * 0,1s = 100000
//mit Prescaler 2: 50000
//Timer-Reload-Wert = 0x10000 - 50000 = 0x3CB0
unsigned char timer_high = 0x3C;
unsigned char timer_low = 0xB0;

void time_update(void);

void high_prior_InterruptHandler(void);
void low_prior_InterruptHandler(void);

void time_update(void)
{
	
}

#pragma code high_prior_InterruptVector = 0x08
void high_prior_InterruptVector(void)
{
	_asm goto high_prior_InterruptHandler
		_endasm
}

#pragma code low_prior_InterruptVector = 0x18
void low_prior_InterruptVector(void)
{
	_asm goto low_prior_InterruptHandler
		_endasm
}

#pragma code init = 0x30
void init(void)
{
#ifndef Simulator // LCD-Initialisierung mit Portzuweisung RA<3:1> und RD<3:0>
	lcd_init();
	lcd_clear();
#endif
	Sekunde = 0; // Uhrzeit initialisieren
	Minute = 0;
	Stunde = 0;

	// lcd initialisieren
	// PORTB und RB1 aus Ausgang
	PORTB = 0x00;
	TRISBbits.TRISB1 = 0;
	TRISBbits.TRISB3 = 1;

	// Timer1 16 bit zugriff kein prescaler 0b10000000
	T1CON = 0x84;

	// TIMER3 to konfigurieren, dass 100ms Intervalle entstehen und TIMER1 für den CPP2-Capture verwendet wird
	//  Timer3, 16-bit mode, 1:8 prescaler 1000 0000
	T3CON = 0x94;

	// Set Timer3 period
	TMR3H = timer_high; // High byte
	TMR3L = timer_low; // Low byte

	// CPP2 Modul im Capture-Modus betreiben 0000 0101
	CCP2CON = 0x05;

	// Interrupt configurieren
	// Enable priority levels on interrupts
	RCONbits.IPEN = 1; 
	// Enable global interrupts
	INTCONbits.GIEH = 1;
	INTCONbits.GIEL = 1;
	// Interrupts aktivieren 1100 0000
	//INTCON = 0xC0;

	IPR2bits.CCP2IP = 1; //high priority
	PIR2bits.CCP2IF = 0;
	PIE2bits.CCP2IE = 1;

	// Enable Timer3 overflow interrupt
	IPR2bits.TMR3IP = 0; //low priority
	PIR2bits.TMR3IF = 0;
	PIE2bits.TMR3IE = 1;

	T1CONbits.TMR1ON = 1; // Timer1 starten
	T3CONbits.TMR3ON = 1; // Timer3 starten

}

// hochpriorisierte ISR:
// Messung der Dauer des Echoimpulses (an RB3) durch CCP2 Modul
// Steigende Flanke an RB3/CCP2: Capture-Wert speichern
// Fallende Flanke: Differenz mit gespeichertem Wert bilden
#pragma code
#pragma interrupt high_prior_InterruptHandler
void high_prior_InterruptHandler(void)
{
	//0101 = Capture mode, every rising edge
	if (CCP2CON == 0x05)
	{
		//Capture-Werte
		caputure_werte = CCPR2;
		//TIMER1 IF zur?cksetzen
		PIR1bits.TMR1IF = 0;
		//0100 = Capture mode, every falling edge
		CCP2CON = 0x04;
	}
	else
	{
		//TIMER1 Overflow
		if (PIR1bits.TMR1IF == 1)
		{
			//Maximalwert
			Abstand = 0xFFFF;
		}
		else
		{
			//abstrand berechnen
			Abstand = (CCPR2 - caputure_werte) / 58;
		}
		//flankenrichtung steigende
		CCP2CON = 0x05;
	}
	//interrupt-flag zur?cksetzen
	PIR2bits.CCP2IF = 0;
}

// niedrigpriorisierte ISR:
// 100ms-Intervalle von Timer 3 verwenden, um die Abstandsmessung darzustellen.
// Die Intervalle dienen zugleich als Zeitbasis f?r die Uhr.
#pragma code
#pragma interruptlow low_prior_InterruptHandler
void low_prior_InterruptHandler(void)
{
	// Siehe Flussdiagramm:
	// Startwert f?r 100ms Intervalle in Timer3 laden

//	TMR3H = timer_high;
//	TMR3L = timer_low;
	TMR3H = 0x3C;
	TMR3L = 0xB0;

	if (Abstand != 0xFFFF)
	{
		sprintf(LCDtext1, (const far rom char *)"Abstand: %3dcm  ", Abstand); // Abstand anzeigen
	}
	else
	{
		sprintf(LCDtext1, (const far rom char *)"Abstand: ---    "); // kein Messwert vorhanden
	}

#ifndef Simulator
	lcd_gotoxy(1, 1);	  // LCD Zeile 1 ausgeben:
	lcd_printf(LCDtext1); // LCDtext1 Abstand: ...
#endif

	// Z?hlung der Echtzeit-Uhr
	Vorzaehler++;
	if (Vorzaehler == 10)
	{
		Vorzaehler = 0;
		Sekunde++;
		if (Sekunde == 60) // bei 60 Sekunden
		{
			Sekunde = 0;	  // Sekunden auf 0 setzen
			Minute++;		  // Minuten hochz?hlen
			if (Minute == 60) // bei 60 Minuten
			{
				Minute = 0;		  // Minuten auf 0 setzen
				Stunde++;		  // Stunden hochz?hlen
				if (Stunde == 24) // bei 24 Stunden
					Stunde = 0;	  // Stunden auf 0 setzen
			}
		}
	}
	// Jede Sekunde die Uhrzeit anzeigen
	sprintf(LCDtext2, (const far rom char *)"Zeit: %02d:%02d:%02d  ", Stunde, Minute, Sekunde);

#ifndef Simulator
	// LCD Zeile 2 ausgeben:
	lcd_gotoxy(2, 1);
	// LCDtext2 Zeit: ...
	lcd_printf(LCDtext2);
#endif

	// weiter siehe Flussdiagramm ...
	TMR1H = 0;
	TMR1L = 0;

	PORTBbits.RB1 = 1;
	
	Nop();	
	Nop();
	Nop();	
	Nop();	
	Nop();
	Nop();	
	Nop();
	Nop();	
	Nop();	
	Nop();

	PORTBbits.RB1 = 0;

	PIR2bits.TMR3IF = 0;
}
void main()
{
	init();
	while (1)
		;
}