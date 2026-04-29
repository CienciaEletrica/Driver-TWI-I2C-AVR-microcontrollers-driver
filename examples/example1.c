/**
 * @file example1.c
 * @brief Exemplo 1: Operação Master simples (Escrita e Leitura assíncrona).
 * 
 * Este exemplo demonstra como inicializar o driver no modo Master e realizar
 * operações de escrita e leitura em um Slave (0x10) utilizando callbacks.
 * 
 * @details 
 * **Fluxo de execução:**
 * 1. Inicializa a UART para monitoramento.
 * 2. Inicializa o TWI como Master a 100kHz.
 * 3. Registra a função `on_complete` para tratar o fim de cada transação.
 * 4. Dispara uma escrita e, após um atraso, dispara uma leitura.
 * 5. O processamento dos resultados ocorre inteiramente dentro do callback.
 */

#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include "twi.h"

/* Protótipos de funções locais */
void USART_Init(void);
void USART_Transmite_String(const char *str);

/**
 * @brief Callback de conclusão para transações Master.
 * @details Esta função é chamada pela ISR do TWI assim que uma transação termina,
 * seja por sucesso ou erro. 
 * 
 * @warning Como esta função é executada dentro de um contexto de interrupção,
 * ela deve ser o mais rápida possível.
 * 
 * @param st   Status final da operação (@ref TWI_Status_t).
 * @param data Ponteiro para os dados lidos (NULL se a operação foi de escrita).
 * @param len  Quantidade de bytes recebidos.
 */
void on_complete(TWI_Status_t st, const uint8_t* data, size_t len);

/**
 * @brief Ponto de entrada principal da aplicação.
 */
int main(void) {
    /* Inicialização do sistema */
    USART_Init(); 
    USART_Transmite_String("Inicializando...");
    /* Habilita interrupções globais (obrigatório para o funcionamento do TWI) */
    sei(); 

    /* Inicializa o TWI como Master a 100kHz */
    twi_init(TWI_MODE_MASTER, 0, TWI_CLOCK_100KHZ);

    /* Registra o callback de conclusão */
    twi_master_register_callbacks(on_complete, NULL);

    /* 
     * Passo 1: Enviar 3 bytes para o Slave 0x10.
     * A função retorna imediatamente; o envio ocorre via interrupção.
     */
    uint8_t dados[] = {0xAA, 0xBB, 0xCC};
    twi_master_start_write(0x10, dados, 3);
    
    _delay_ms(1000);  // Apenas para separar os testes visualmente

    /* 
     * Passo 2: Ler 4 bytes do Slave 0x10.
     * O buffer deve permanecer válido até que o callback seja chamado.
     */
    uint8_t buffer[4] = {0};
    twi_master_start_read(0x10, buffer, 4);

    while (1) {
        /* O processamento de dados acontece de forma reativa no callback on_complete */
    }
}

/**
 * @brief Inicializa a UART0 para 9600 bps (8N1).
 */
void USART_Init(void){
    uint16_t ubrr = 103;// Para 16MHz e 9600 baud
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;    
    UCSR0B = (1 << TXEN0); // Habilita transmissão
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // Formato do frame: 8 bits, 1 stop bit, sem paridade
}

/**
 * @brief Envia uma string via UART0.
 * @param str Ponteiro para a string (null-terminated).
 */
void USART_Transmite_String(const char *str){
    while (*str)
    {
        while (!(UCSR0A & (1 << UDRE0))); // Aguarda o registrador ficar livre
        UDR0 = *str++;
    }
}

/**
 * @brief Implementação do callback de conclusão.
 */
void on_complete(TWI_Status_t st, const uint8_t* data, size_t len) {
    if (st == TWI_OK) {
        USART_Transmite_String("Sucesso! ");

        /* Verifica se há dados lidos para exibir */
        if (data && len > 0) {
            USART_Transmite_String("Lidos: ");
            for (size_t i = 0; i < len; i++) {
                char buf[8];
                sprintf(buf, "%02X ", data[i]);
                USART_Transmite_String(buf);
            }
        }
        USART_Transmite_String("\r\n");
    } else {
         /* Tratamento de erros */
        USART_Transmite_String("Erro: ");
        if (st == TWI_ERROR_TIMEOUT) USART_Transmite_String("Timeout");
        else if (st == TWI_ERROR_NACK) USART_Transmite_String("NACK");
        else if (st == TWI_ERROR_BUS_BUSY) USART_Transmite_String("Bus Busy");
        else USART_Transmite_String("Outro");
        USART_Transmite_String("\r\n");
    }
}
