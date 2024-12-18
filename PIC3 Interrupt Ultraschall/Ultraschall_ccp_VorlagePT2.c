 //Relatório de Código: Medidor Ultrassônico com PIC18F452
 //Projeto realizado na Hochschule Mannheim
 //Jennifer Azevedo Batista e Guilherme Correa Koller
 //Curso: Digital- und Mikrocomputertechnik (Microcontrolados)


#include <stdio.h>
#include <stdlib.h>

#pragma config OSC = HS, WDT = OFF, LVP = OFF, CCP2MUX = OFF // Configurações do microcontrolador (HS Ocilador, Watchdog //Timer disabled, Low Voltage Programming)

#define Simulator // Ativa o modo de simulação para teste no MPLAB.
#include "p18f452.h" // Arquivo de cabeçalho do PIC18F452.
#include "lcd.h" // Biblioteca para controle do display LCD.

// Variáveis globais para exibição no LCD.
char LCDtext1[20] = " Interrupt      "; // Linha 1 do LCD.
char LCDtext2[20] = " Ultraschall    "; // Linha 2 do LCD.

// Variáveis de distância e tempo.
unsigned int Abstand = 0; // Distância medida pelo sensor.

unsigned char Vorzaehler = 0; // Contador auxiliar para o RTC.
unsigned char Stunde = 0; // Horas.
unsigned char Minute = 0; // Minutos.
unsigned char Sekunde = 0; // Segundos.

unsigned int caputure_werte = 0; // Armazena o valor capturado do Timer1.
unsigned int i = 0;
//100000 contagens (F4240) -> FFFF - F424 = 3035
//Fator de divisão: 1MHz * 100ms = 1000000Hz * 0,1s = 100000
//com Prescaler 2: 50000
//Valor de recarga do temporizador = 0x10000 - 50000 = 0x3CB0
unsigned char timer_high = 0x3C; // Byte alto para inicialização do Timer3.
unsigned char timer_low = 0xB0; // Byte baixo para inicialização do Timer3.

void time_update(void);

// Prototipação de funções.
void high_prior_InterruptHandler(void);
void low_prior_InterruptHandler(void);


// Vetor de interrupção de alta prioridade.
#pragma code high_prior_InterruptVector = 0x08
void high_prior_InterruptVector(void) {
    _asm goto high_prior_InterruptHandler 
_endasm
}

// Vetor de interrupção de baixa prioridade.
#pragma code low_prior_InterruptVector = 0x18
void low_prior_InterruptVector(void) {
    _asm goto low_prior_InterruptHandler 
_endasm
}

// Função de inicialização.
#pragma code init = 0x30

void init(void) {
#ifndef Simulator
    lcd_init(); // Inicializa o LCD.
    lcd_clear(); // Limpa o LCD.
#endif
    Sekunde = 0;
    Minute = 0;
    Stunde = 0;

    PORTB = 0x00; // Inicializa o PORTB com valor 0.
    TRISBbits.TRISB1 = 0; // Configura RB1 como saída para o trigger.
    TRISBbits.TRISB3 = 1; // Configura RB3 como entrada para o eco.

    T1CON = 0x84; // Configura o Timer1 para captura de tempo.
    T3CON = 0x94; // Configura o Timer3 para interrupções a cada 100ms.
    TMR3H = timer_high;
    TMR3L = timer_low;

    CCP2CON = 0x05; // Configura o CCP2 no modo Capture (flanco de subida).

    RCONbits.IPEN = 1; // Ativa níveis de prioridade de interrupções.
    INTCONbits.GIEH = 1; // Ativa interrupções de alta prioridade.
    INTCONbits.GIEL = 1; // Ativa interrupções de baixa prioridade.


    IPR2bits.CCP2IP = 1; // Configura CCP2 como alta prioridade.
    PIR2bits.CCP2IF = 0;
    PIE2bits.CCP2IE = 1; // Habilita interrupções do CCP2.


    IPR2bits.TMR3IP = 0; // Configura Timer3 como baixa prioridade.
    PIR2bits.TMR3IF = 0;
    PIE2bits.TMR3IE = 1; // Habilita interrupções do Timer3.

    T1CONbits.TMR1ON = 1; // Ativa o Timer1.
    T3CONbits.TMR3ON = 1; // Ativa o Timer3.
}

// Interrupção de alta prioridade.
// ISR de alta prioridade 
// Medição da duração do pulso de eco (em RB3) pelo módulo CCP2
// Borda ascendente em RB3/CCP2: Salvar valor de captura
// Borda descendente: diferença de forma com valor armazenado
#pragma code
#pragma interrupt high_prior_InterruptHandler
void high_prior_InterruptHandler(void) {
    if (CCP2CON == 0x05) { // Captura no flanco de subida.
        caputure_werte = CCPR2; // Salva o valor capturado.
        PIR1bits.TMR1IF = 0; // Reseta a flag de overflow do Timer1.
        CCP2CON = 0x04; // Configura para capturar no flanco de descida.
    } 
    else 
    {
        if (PIR1bits.TMR1IF == 1) { // Verifica overflow do Timer1.
            Abstand = 0xFFFF; // Define distância máxima.
        } 
        else
        {
            Abstand = (CCPR2 - caputure_werte) / 58; // Calcula a distância em cm.
        }
        CCP2CON = 0x05; // Retorna para captura no flanco de subida.
    }
    PIR2bits.CCP2IF = 0; // Reseta a flag de interrupção do CCP2.
}

// Interrupção de baixa prioridade.
// IST de baixa prioridade:
// Usa intervalos de 100 ms do temporizador 3 para exibir a medição de distância. 
// Os intervalos também servem como base de tempo para o relógio. 
#pragma code
#pragma interruptlow low_prior_InterruptHandler
void low_prior_InterruptHandler(void) {
    TMR3H = timer_high; // Reinicia o Timer3.
    TMR3L = timer_low;

    TMR3H = 0x3C;
    TMR3L = 0xB0;

    if (Abstand != 0xFFFF) {
        sprintf(LCDtext1, (const far rom char *)"Abstand: %3dcm  ", Abstand); // Formata a distância.
    } 
    else 
    {
        sprintf(LCDtext1, (const far rom char *)"Abstand: ---    "); // Exibe "---" se não houver medição.
    }

#ifndef Simulator
    lcd_gotoxy(1, 1); // Posiciona no LCD linha 1.
    lcd_printf(LCDtext1); // Exibe distância no LCD.
#endif
  // Contando o relógio em tempo real 
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

    sprintf(LCDtext2, (const far rom char *)"Zeit: %02d:%02d:%02d  ", Stunde, Minute, Sekunde); // Formata o horário.

#ifndef Simulator
    lcd_gotoxy(2, 1); // Posiciona no LCD linha 2.
    lcd_printf(LCDtext2); // Exibe o horário no LCD.
#endif

    TMR1H = 0; // Reseta Timer1 para próxima medição.
    TMR1L = 0;

    PORTBbits.RB1 = 1; // Envia o pulso de trigger.
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

    PORTBbits.RB1 = 0; // Finaliza o trigger.

    PIR2bits.TMR3IF = 0; // Reseta a flag de interrupção do Timer3.
}

// Função principal.
void main(void) {
    init(); // Inicializa o sistema.
    while (1); // Loop infinito para manter o sistema em operação.
}
