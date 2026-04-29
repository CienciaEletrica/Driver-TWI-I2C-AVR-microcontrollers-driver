/**
 * @file example2.c
 * @brief Exemplo 2: Operação Slave completa (Responde ao Master do Exemplo 1).
 * 
 * Este exemplo demonstra como configurar o AVR para atuar como um dispositivo 
 * escravo no barramento I2C, tratando eventos de recepção e requisição de dados.
 * 
 * @details 
 * **Funcionamento:**
 * 1. O Slave é inicializado com o endereço @c 0x10.
 * 2. Quando o Master envia dados (@c SLA+W), a função `on_receive` é disparada.
 * 3. Quando o Master solicita dados (@c SLA+R), a função `on_transmit` é disparada
 *    para preencher o buffer de saída.
 * 
 * Toda a lógica é baseada em eventos (callbacks) disparados pela ISR.
 */

#include <avr/io.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include "twi.h"

/* Protótipos das funções auxiliares */
void USART_Init(void);
void USART_Transmite_String(const char *str);

/**
 * @brief Callback de recebimento (Master -> Slave).
 * @details Chamado automaticamente quando o Master finaliza uma escrita para este Slave.
 * @param dados Ponteiro para o buffer interno contendo os bytes recebidos.
 * @param len Quantidade de bytes recebidos.
 */
void on_receive(const uint8_t* dados, size_t len);

/**
 * @brief Callback de transmissão (Slave -> Master).
 * @details Chamado quando o Master solicita dados deste Slave. A aplicação deve 
 * preencher o @p buffer com as informações desejadas.
 * @param buffer Ponteiro para o buffer de transmissão do hardware.
 * @param max_len Espaço máximo disponível no buffer (definido em @ref TWI_RX_BUFFER_SIZE).
 * @return size_t Quantidade de bytes efetivamente carregados no buffer.
 */
size_t on_transmit(uint8_t* buffer, size_t max_len);

/**
 * @brief Ponto de entrada do Slave.
 */
int main(void) {
    USART_Init(); // Inicializa UART

    /* Habilita interrupções (necessário para o TWI reagir ao Master) */
    sei();
    
    /** 
     * @brief Inicializa como Slave no endereço 0x10.
     * O clock aqui define a taxa de amostragem interna, mas o clock do barramento 
     * é ditado pelo Master.
     */
    twi_init(TWI_MODE_SLAVE, 0x10, TWI_CLOCK_100KHZ);

    /* Registra os callbacks de evento do Slave */
    twi_slave_register_callbacks(on_receive, on_transmit);

    USART_Transmite_String("Slave pronto - endere�o 0x10\n");

    while (1) {
        /* 
         * O loop principal permanece livre para outras tarefas.
         * A comunicação TWI não consome tempo de CPU enquanto o barramento está ocioso.
         */
    }
}

/**
 * @brief Inicializa a UART para monitoramento.
 */
void USART_Init(void){
    uint16_t ubrr = 103; // 9600 baud para 16MHz
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;    
    UCSR0B = (1 << TXEN0); // Habilita transmissão
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // Formato do frame: 8 bits, 1 stop bit, sem paridade
}

// --- Transmissão de uma string ---
void USART_Transmite_String(const char *str){
    while (*str)
    {
        while (!(UCSR0A & (1 << UDRE0))); // Aguarda o registrador ficar livre
        UDR0 = *str++;
    }
}

/**
 * @brief Implementação do tratamento de dados recebidos.
 */
void on_receive(const uint8_t* dados, size_t len) {
    USART_Transmite_String("Recebi ");
    for (size_t i = 0; i < len; i++) {
        char buf[8];
        sprintf(buf, "%02X ", dados[i]);
        USART_Transmite_String(buf);
    }
    USART_Transmite_String("\r\n");

    /* Exemplo de lógica de comando: se receber 0x01, executa ação */
    if (len >= 1 && dados[0] == 0x01) {
        USART_Transmite_String("Comando 0x01 recebido!\r\n");
    }
}

/**
 * @brief Implementação da resposta ao Master.
 */
size_t on_transmit(uint8_t* buffer, size_t max_len) {
    USART_Transmite_String("Master pediu dados\n");

    /* Exemplo: Enviando uma sequência de bytes fixa */
    if (max_len >= 5) {
        buffer[0] = 0x11;
        buffer[1] = 0x22;
        buffer[2] = 0x33;
        buffer[3] = 0x44;
        buffer[4] = 0x55;
        return 5;  /* Retorna 5 para indicar que há 5 bytes prontos para o envio */
    }

    return 0;  /* Se retornar 0, o hardware enviará um NACK ao Master */
}
