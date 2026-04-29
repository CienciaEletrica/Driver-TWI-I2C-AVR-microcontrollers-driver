/**
 * @file example3.c
 * @brief Exemplo 3: Leitura do registrador WHO_AM_I do sensor MPU6050.
 * 
 * Este exemplo demonstra o uso da transação combinada (Escrita seguida de Leitura)
 * utilizando a técnica de **Repeated Start**, evitando que outros Masters tomem
 * o barramento entre as operações.
 * 
 * @details 
 * **Sequência Lógica da Transação:**
 * 1. **START**: O Master assume o barramento.
 * 2. **SLA+W (0x68)**: Endereça o MPU6050 para escrita.
 * 3. **DATA (0x75)**: Envia o endereço do registrador `WHO_AM_I`.
 * 4. **REPEATED START**: Reinicia o protocolo sem liberar o barramento.
 * 5. **SLA+R (0x68)**: Endereça o MPU6050 para leitura.
 * 6. **DATA RX**: Lê o valor do registrador (esperado 0x68).
 * 7. **STOP**: Libera o barramento.
 */

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include "twi.h"

/* Protótipos de funções de auxílio */
void USART_Init(void);
void USART_Transmite_String(const char *str);
void uart_puthex8(uint8_t val);

/**
 * @brief Callback chamado ao fim da transação combinada.
 * @param st Status da operação (@ref TWI_Status_t).
 * @param data Ponteiro para o buffer de recepção com o valor do WHO_AM_I.
 * @param len Quantidade de bytes lidos (esperado 1).
 */
void on_complete(TWI_Status_t st, const uint8_t* data, size_t len);

/**
 * @brief Função principal para o teste do MPU6050.
 */
int main(void) {
    /* Inicializa UART para monitoramento via Terminal Serial */
    USART_Init();
    USART_Transmite_String("\r\n=== Teste Repeated Start - MPU6050 ===\r\n");

    /* Habilita interrupções globais - Essencial antes de twi_init */
    sei();
    
    /* Inicializa o barramento TWI a 100 kHz */
    if (twi_init(TWI_MODE_MASTER, 0, TWI_CLOCK_100KHZ) != TWI_OK) {
        USART_Transmite_String("Falha ao inicializar TWI!\r\n");
        while (1);
    }

    /* Registra o callback de conclusão */
    twi_master_register_callbacks(on_complete, NULL);  // NULL = sem callback separado para erro

    /** @brief Endereço do registrador WHO_AM_I no MPU6050. */
    uint8_t reg_addr = 0x75;

    /** @brief Buffer estático para garantir persistência durante a ISR. */
    uint8_t who_am_i[1] = {0};

    /** 
     * @brief Inicia transação Write-then-Read.
     * Envia o endereço do registrador e lê a resposta de forma atômica.
     */
    twi_master_start_write_read(
        0x68,           /**< Endereço I2C padrão do MPU6050. */
        &reg_addr,      /**< Buffer com endereço do registrador. */
        1,              /**< Tamanho da escrita (1 byte). */
        who_am_i_val,   /**< Buffer para salvar a leitura. */
        1               /**< Tamanho da leitura (1 byte). */
    );

    // A partir daqui o programa continua normalmente
    // O callback on_complete ser� chamado pela ISR quando terminar

    while (1) {
         /* O sistema permanece livre; o resultado será impresso pelo callback. */
        _delay_ms(500);
    }
}

/**
 * @brief Inicializa a UART para depuração.
 */
void USART_Init(void){
    uint16_t ubrr = 103; // 9600 bps @ 16MHz
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;    
    UCSR0B = (1 << TXEN0); // Habilita transmissão
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // Formato do frame: 8 bits, 1 stop bit, sem paridade
}

void USART_Transmite_String(const char *str){
    while (*str)
    {
        while (!(UCSR0A & (1 << UDRE0))); // Aguarda o registrador ficar livre
        UDR0 = *str++;
    }
}

/**
 * @brief Converte um byte em Hexadecimal e envia via UART.
 * @param val Valor a ser convertido.
 */
void uart_puthex8(uint8_t val) {
    char buf[5];
    sprintf(buf, "%02X ", val);
    USART_Transmite_String(buf);
}

/**
 * @brief Implementação do callback de conclusão da transação.
 */
void on_complete(TWI_Status_t st, const uint8_t* data, size_t len) {
    if (st == TWI_OK) {
        USART_Transmite_String("Sucesso! WHO_AM_I = ");
        if (len >= 1) {
            uart_puthex8(data[0]);  /**< Esperado: 0x68. */
        }
        USART_Transmite_String("\r\n");
    } else {
        USART_Transmite_String("Erro na transa��o: ");
        if (st == TWI_ERROR_TIMEOUT) USART_Transmite_String("Timeout");
        else if (st == TWI_ERROR_NACK) USART_Transmite_String("NACK");
        else if (st == TWI_ERROR_BUS_BUSY) USART_Transmite_String("Bus Busy");
        else USART_Transmite_String("Outro erro");
        USART_Transmite_String("\r\n");
    }
}
