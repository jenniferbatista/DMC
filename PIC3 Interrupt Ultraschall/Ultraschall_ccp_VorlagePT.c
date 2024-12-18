/* ***************************************
 * Relatório de Código: Medidor Ultrassônico com PIC18F452
 * Projeto realizado na Hochschule Mannheim
 * Curso: Digital- und Mikrocomputertechnik
 * Data: 13.12.2023
 * *************************************** */

#include <stdio.h>
#include <stdlib.h>

#pragma config OSC = HS, WDT = OFF, LVP = OFF, CCP2MUX = OFF // Configurações do microcontrolador

// Modo de simulação - descomente para uso com hardware
#define Simulator

// Includes de bibliotecas específicas
#include "p18f452.h"
#include "lcd.h"

// Variáveis globais
char LCDtext1[20] = " Interrupt      "; // Linha 1 do LCD
char LCDtext2[20] = " Ultraschall    "; // Linha 2 do LCD

unsigned int Abstand = 0; // Variável para armazenar a distância medida

unsigned char Vorzaehler = 0; // Variáveis para o relógio (segundos, minutos, horas)
unsigned char Stunde = 0;
unsigned char Minute = 0;
unsigned char Sekunde = 0;

unsigned int caputure_werte = 0; // Valor capturado do pulso Echo

// Timer3: configurado para intervalos de 100ms
unsigned char timer_high = 0x3C;
unsigned char timer_low = 0xB0;

void time_update(void);
void high_prior_InterruptHandler(void); // ISR de alta prioridade
void low_prior_InterruptHandler(void);  // ISR de baixa prioridade

/* *******************************************
 * Função init: Configuração inicial
 * *******************************************/
#pragma code init = 0x30
void init(void)
{
#ifndef Simulator // Inicialização do LCD
    lcd_init();
    lcd_clear();
#endif

    // Inicialização do relógio
    Sekunde = 0;
    Minute = 0;
    Stunde = 0;

    // Configuração dos pinos
    PORTB = 0x00;
    TRISBbits.TRISB1 = 0; // Pino RB1 (Trigger) como saída
    TRISBbits.TRISB3 = 1; // Pino RB3 (Echo) como entrada

    // Configuração do Timer1 para o CCP2
    T1CON = 0x84; // Habilita o Timer1, sem prescaler

    // Configuração do Timer3 para intervalos de 100ms
    T3CON = 0x94; // Timer3 habilitado, 1:8 prescaler
    TMR3H = timer_high; // Define o valor inicial do Timer3 (high byte)
    TMR3L = timer_low;  // Define o valor inicial do Timer3 (low byte)

    // Configuração do CCP2 no modo Capture
    CCP2CON = 0x05; // Modo Capture, flanco de subida

    // Configuração das interrupções
    RCONbits.IPEN = 1; // Habilita prioridade de interrupção
    INTCONbits.GIEH = 1; // Habilita interrupções de alta prioridade
    INTCONbits.GIEL = 1; // Habilita interrupções de baixa prioridade

    // Configurações específicas das interrupções
    IPR2bits.CCP2IP = 1; // CCP2 em alta prioridade
    PIE2bits.CCP2IE = 1; // Habilita interrupção CCP2

    IPR2bits.TMR3IP = 0; // Timer3 em baixa prioridade
    PIE2bits.TMR3IE = 1; // Habilita interrupção do Timer3

    // Habilita Timer1 e Timer3
    T1CONbits.TMR1ON = 1;
    T3CONbits.TMR3ON = 1;
}

/* *******************************************
 * ISR de Alta Prioridade: Captura o tempo do pulso Echo
 * *******************************************/
#pragma code
#pragma interrupt high_prior_InterruptHandler
void high_prior_InterruptHandler(void)
{
    if (CCP2CON == 0x05) // Flanco de subida
    {
        caputure_werte = CCPR2; // Salva o valor capturado
        PIR1bits.TMR1IF = 0;    // Reseta o Timer1
        CCP2CON = 0x04;         // Configura para flanco de descida
    }
    else // Flanco de descida
    {
        if (PIR1bits.TMR1IF == 1) // Caso Timer1 tenha overflow
        {
            Abstand = 0xFFFF; // Define distância máxima
        }
        else
        {
            Abstand = (CCPR2 - caputure_werte) / 58; // Calcula a distância em cm
        }
        CCP2CON = 0x05; // Volta para flanco de subida
    }
    PIR2bits.CCP2IF = 0; // Reseta a flag de interrupção CCP2
}

/* *******************************************
 * ISR de Baixa Prioridade: Atualiza o LCD e o relógio
 * *******************************************/
#pragma code
#pragma interruptlow low_prior_InterruptHandler
void low_prior_InterruptHandler(void)
{
    TMR3H = timer_high; // Reinicia o Timer3
    TMR3L = timer_low;

    // Atualiza a distância no LCD
    if (Abstand != 0xFFFF)
    {
        sprintf(LCDtext1, (const far rom char *)"Abstand: %3dcm  ", Abstand);
    }
    else
    {
        sprintf(LCDtext1, (const far rom char *)"Abstand: ---    ");
    }

#ifndef Simulator
    lcd_gotoxy(1, 1);
    lcd_printf(LCDtext1);
#endif

    // Atualiza o relógio
    Vorzaehler++;
    if (Vorzaehler == 10)
    {
        Vorzaehler = 0;
        Sekunde++;
        if (Sekunde == 60)
        {
            Sekunde = 0;
            Minute++;
            if (Minute == 60)
            {
                Minute = 0;
                Stunde++;
                if (Stunde == 24)
                    Stunde = 0;
            }
        }
    }

    sprintf(LCDtext2, (const far rom char *)"Zeit: %02d:%02d:%02d  ", Stunde, Minute, Sekunde);

#ifndef Simulator
    lcd_gotoxy(2, 1);
    lcd_printf(LCDtext2);
#endif

    TMR1H = 0; // Reseta Timer1
    TMR1L = 0;

    // Envia trigger para o sensor
    PORTBbits.RB1 = 1;
    Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop();
    PORTBbits.RB1 = 0;

    PIR2bits.TMR3IF = 0; // Reseta a flag do Timer3
}

/* *******************************************
 * Função Principal: Inicializa o sistema e aguarda interrupções
 * *******************************************/
void main()
{
    init(); // Inicializa configurações
    while (1);
}
