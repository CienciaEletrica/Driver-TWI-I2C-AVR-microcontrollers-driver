/**
 * @file twi.c
 * @brief Implementação do Driver TWI/I2C para microcontroladores AVR.
 * @author Seu Nome / Empresa
 * @date 2024
 * 
 * Este arquivo contém a lógica da máquina de estados para operações
 * Master (Transmissor/Receptor) e Slave (Transmissor/Receptor) utilizando interrupções.
 */
#include "twi.h"
#include "twi_internal.h"
#include "twi_config.h"
#include <stddef.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/twi.h>  // Para macros como TW_START, TW_MT_SLA_ACK
/** 
 * @brief Limite padrão de timeout para evitar travamento do barramento.
 * @details Aproximadamente 200 interrupções sem progresso (100-200ms em 100kHz).
 */
#define TWI_TIMEOUT_LIMIT_DEFAULT  200   // ~200 interrupções sem progresso ? 100-200 ms em 100 kHz

/** @brief Buffer interno para recepção de dados no modo Slave. */
static uint8_t slave_internal_rx_buffer[TWI_RX_BUFFER_SIZE];
/** @brief Buffer interno para transmissão de dados no modo Slave. */
static uint8_t slave_internal_tx_buffer[TWI_RX_BUFFER_SIZE];
/** @brief Índice atual do buffer de recepção do Slave. */
static size_t slave_rx_idx = 0;

/** 
 * @brief Estrutura global que armazena o estado da transação atual.
 * Marcada como volatile pois é modificada dentro da ISR.
 */
volatile TWI_Transaction_t g_twi_txn = { .state = TWI_STATE_IDLE };

/** @name Callbacks
 * Ponteiros para funções de retorno de chamada do usuário.
 * @{ */
static TWI_Master_Complete_Callback_t master_complete_cb = NULL;
static TWI_Error_Callback_t error_cb = NULL;
static TWI_Slave_Receive_Callback_t slave_receive_cb = NULL;
static TWI_Slave_Transmit_Callback_t slave_transmit_cb = NULL;
/** @} */

#if (TWI_DEBUG_MODE == 1)
/** @brief Armazena o último status do registrador TWSR para depuração. */
volatile uint8_t debug_twi_last_status = 0;
/** @brief Armazena o último estado lógico da transação para depuração. */
volatile TWI_State_t debug_twi_last_state = TWI_STATE_IDLE;
/** @brief Contador de interrupções disparadas. */
volatile uint8_t debug_twi_counter = 0;
#endif

/**
 * @brief Calcula o valor do registrador TWBR baseado na frequência de CPU e SCL desejada.
 * @param clock Enumeração TWI_Clock_t (100kHz ou 400kHz).
 * @return uint8_t Valor calculado para o registrador TWBR.
 */
static uint8_t twi_calc_twbr(TWI_Clock_t clock) {
    uint32_t scl_freq = (clock == TWI_CLOCK_400KHZ) ? 400000UL : 100000UL;
    return (uint8_t)((F_CPU / scl_freq) - 16UL) / 2UL;
}

// Implementação da inicialização do hardware
TWI_Status_t twi_init(TWI_Mode_t mode, uint8_t address, TWI_Clock_t clock) {
    if (mode != TWI_MODE_MASTER && mode != TWI_MODE_SLAVE) return TWI_ERROR_INVALID_ARG;

    g_twi_txn.timeout_counter = 0;
    g_twi_txn.timeout_limit   = TWI_TIMEOUT_LIMIT_DEFAULT;
    
    // Configura clock
    TWBR = twi_calc_twbr(clock);
    TWSR = 0;  // Prescaler 1

    // Habilita TWI e interrupções
    TWCR = (1 << TWEN) | (1 << TWIE);

    if (mode == TWI_MODE_SLAVE) {
        TWAR = (address << 1) | (TWI_ENABLE_GENERAL_CALL ? 1 : 0);  // Endereço + GCA
        TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWEA);  // Habilita ACK para Slave
    }

    g_twi_txn.state = TWI_STATE_IDLE;
    return TWI_OK;
}

/**
 * @brief Desabilita o módulo TWI e reseta o estado da transação.
 */
void twi_deinit(void) {
    TWCR = 0;                  // Desabilita o hardware, interrupções e o ACK
    g_twi_txn.state = TWI_STATE_IDLE;
    g_twi_txn.status = TWI_OK;
}

// Registra os callbacks para o modo Master.
void twi_master_register_callbacks(TWI_Master_Complete_Callback_t on_complete, TWI_Error_Callback_t on_error) {
    master_complete_cb = on_complete;
    error_cb = on_error;
}

// Registra os callbacks para o modo Slave.
void twi_slave_register_callbacks(TWI_Slave_Receive_Callback_t on_receive, TWI_Slave_Transmit_Callback_t on_transmit) {
    slave_receive_cb = on_receive;
    slave_transmit_cb = on_transmit;
}

// Inicia uma varredura (probe) no barramento para verificar a presença de um Slave.
TWI_Status_t twi_master_start_probe(uint8_t slave_address) {
    if (g_twi_txn.state != TWI_STATE_IDLE) return TWI_ERROR_BUS_BUSY;

    g_twi_txn.state = TWI_STATE_START;
    g_twi_txn.slave_addr = slave_address;
    g_twi_txn.tx_len = 0;  // Sem dados, só probe
    g_twi_txn.rx_len = 0;
    g_twi_txn.on_complete = master_complete_cb;
    g_twi_txn.on_error = error_cb;

    // Inicia START
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN) | (1 << TWIE);
    return TWI_OK;
}

/**
 * @brief Vetor de Interrupção TWI. 
 * @details Gerencia a máquina de estados complexa para Master e Slave,
 * tratando condições de START, ACK/NACK, recepção e transmissão de dados.
 */
ISR(TWI_vect) {
    uint8_t twsr = TW_STATUS;
    g_twi_txn.timeout_counter++;
    
    #if (TWI_DEBUG_MODE == 1)
    debug_twi_last_status = twsr;
    debug_twi_last_state = g_twi_txn.state;
    debug_twi_counter++;
    #endif

    // Verifica timeout (apenas se estamos em transação ativa)
    if (g_twi_txn.state != TWI_STATE_IDLE &&
        g_twi_txn.timeout_counter >= g_twi_txn.timeout_limit) {
        
        g_twi_txn.status = TWI_ERROR_TIMEOUT;
        g_twi_txn.state  = TWI_STATE_ERROR;
        
        // Libera o bus com STOP
        TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN) | (1 << TWIE);
        
        // Opcional: log de debug
        #if (TWI_DEBUG_MODE == 1)
        debug_twi_last_status = 0xFF; // valor especial para indicar timeout
        #endif
        
        // Não continua a execução normal da ISR
        return;
    }
        
    switch (twsr) {
        /* --- Master Start / Repeated Start --- */
        case TW_START: // 0x08
            g_twi_txn.timeout_counter = 0;
            
        case TW_REP_START: // 0x10
            if (g_twi_txn.tx_len > 0 && g_twi_txn.tx_idx < g_twi_txn.tx_len ) {
                TWDR = (g_twi_txn.slave_addr << 1) | TW_WRITE;
                g_twi_txn.state = TWI_STATE_SLA_W;
            } else {
                TWDR = (g_twi_txn.slave_addr << 1) | TW_READ;
                g_twi_txn.state = TWI_STATE_SLA_R;
            }
            TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
            g_twi_txn.timeout_counter = 0;
            break;

        /* --- Master Transmitter --- */
        case TW_MT_SLA_ACK: // 0x18
            g_twi_txn.timeout_counter = 0;
            
        case TW_MT_DATA_ACK: // 0x28
            if (g_twi_txn.tx_idx < g_twi_txn.tx_len) {
                TWDR = g_twi_txn.tx_buf[g_twi_txn.tx_idx++];
                TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
            } else if (g_twi_txn.rx_len > 0) {
                // Repeated Start para leitura
                TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN) | (1 << TWIE);
            } else {
                // Fim da transação: Envia STOP
                g_twi_txn.status = TWI_OK;
                g_twi_txn.state = TWI_STATE_STOP;
                TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN) | (1 << TWIE);
            }
            g_twi_txn.timeout_counter = 0;
            break;
            
        /* --- Master Receiver --- */
        case TW_MR_SLA_ACK: // 0x40
            g_twi_txn.rx_idx = 0;
            // Se vamos ler mais de 1 byte, pede ACK para o primeiro dado
            if (g_twi_txn.rx_len > 1) {
                TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
            } else {
                // Se for apenas 1 byte, respondemos com NACK (TWEA=0) após recebe-lo.
                TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
            }
            g_twi_txn.timeout_counter = 0;
            break;

        case TW_MR_DATA_ACK: // 0x50
            // Recebemos um byte e o Master deu ACK (pediu mais).
            g_twi_txn.rx_buf[g_twi_txn.rx_idx++] = TWDR;

            // Se o PRÓXIMO byte for o Último, devemos responder com NACK (TWEA=0).
            if (g_twi_txn.rx_idx < (g_twi_txn.rx_len - 1)) {
                TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
            } else {
                TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
            }
            g_twi_txn.timeout_counter = 0;
            break;

        case TW_MR_DATA_NACK:    // 0x58
            // Recebemos o último byte (Master deu NACK conforme planejado).
            g_twi_txn.rx_buf[g_twi_txn.rx_idx++] = TWDR; // Salva o último byte
            g_twi_txn.status = TWI_OK;
            g_twi_txn.state = TWI_STATE_STOP;
            TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN) | (1 << TWIE);
            g_twi_txn.timeout_counter = 0;
            break;

        /* --- Slave Receiver --- */
        case TW_SR_SLA_ACK:     // 0x60: Master quer escrever em nós
            g_twi_txn.timeout_counter = 0;
            
        case TW_SR_GCALL_ACK:   // 0x70: Chamada geral recebida
            slave_rx_idx = 0;   // Reseta o índice de recepção
            // Habilita ACK para receber o primeiro byte
            TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
            g_twi_txn.timeout_counter = 0;
            break;

        case TW_SR_DATA_ACK:    // 0x80: Byte recebido com sucesso
            g_twi_txn.timeout_counter = 0;
            
        case TW_SR_GCALL_DATA_ACK:
            if (slave_rx_idx < TWI_RX_BUFFER_SIZE) {
                slave_internal_rx_buffer[slave_rx_idx++] = TWDR;
            }
            // Continua dando ACK enquanto houver espaço no buffer
            uint8_t sr_ack = (slave_rx_idx < TWI_RX_BUFFER_SIZE) ? (1 << TWEA) : 0;
            TWCR = (1 << TWINT) | sr_ack | (1 << TWEN) | (1 << TWIE);
            g_twi_txn.timeout_counter = 0;
            break;

        case TW_SR_STOP: // 0xA0: Master finalizou a escrita (STOP ou REPEATED START)
            if (slave_receive_cb) {
                // Notifica a aplicação com o buffer preenchido com segurança
                slave_receive_cb(slave_internal_rx_buffer, slave_rx_idx);
            }
            slave_rx_idx = 0;  // Limpa para próxima transação
            // Volta a monitorar o barramento
            TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
            g_twi_txn.timeout_counter = 0;
            break;

        // Slave Transmitter
        case TW_ST_SLA_ACK:     // 0xA8: Master quer ler de nós
            g_twi_txn.tx_idx = 0;
            if (slave_transmit_cb) {
                // Aplicação preenche o buffer interno de transmissão
                g_twi_txn.tx_len = slave_transmit_cb(slave_internal_tx_buffer, TWI_RX_BUFFER_SIZE);
            }else {
                g_twi_txn.tx_len = 0;
            }
            // Carrega o primeiro byte do buffer interno estável
            TWDR = (g_twi_txn.tx_len > 0) ? slave_internal_tx_buffer[g_twi_txn.tx_idx++] : 0x00;
            TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
            g_twi_txn.timeout_counter = 0;
            break;

        case TW_ST_DATA_ACK:    // 0xB8: Master recebeu e pediu o próximo
            if (g_twi_txn.tx_idx < g_twi_txn.tx_len) {
                TWDR = slave_internal_tx_buffer[g_twi_txn.tx_idx++];
            } else {
                TWDR = 0x00;    // Envia dummy se o Master pedir mais do que o provido
            }
            TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
            g_twi_txn.timeout_counter = 0;
            break;

        case TW_ST_DATA_NACK:    // 0xC0: Master enviou NACK (Fim da leitura)
            g_twi_txn.timeout_counter = 0;
            
        case TW_ST_LAST_DATA:    // 0xC8: Enviamos o último byte possível
            TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
            // Transação Slave concluída
            g_twi_txn.timeout_counter = 0;
            break;
        
        /* === TRATAMENTO DE ERROS === */
        case TW_MT_SLA_NACK:
            g_twi_txn.timeout_counter = 0;
            
        case TW_MR_SLA_NACK:
            g_twi_txn.status = TWI_ERROR_NACK;
            g_twi_txn.state = TWI_STATE_ERROR;
            TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN) | (1 << TWIE);
            g_twi_txn.timeout_counter = 0;
            break;

        case TW_MT_DATA_NACK:
            g_twi_txn.status = TWI_ERROR_NACK;
            g_twi_txn.state = TWI_STATE_ERROR;
            TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN) | (1 << TWIE);
            g_twi_txn.timeout_counter = 0;
            break;
            
        default:
            TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
            g_twi_txn.timeout_counter = 0;
            break;
    }

    // Finalização das transações Master (conforme seu padrão)
    if (g_twi_txn.state == TWI_STATE_STOP || g_twi_txn.state == TWI_STATE_ERROR) {
        TWI_Status_t res = g_twi_txn.status;
        g_twi_txn.state = TWI_STATE_IDLE;
        if (res == TWI_OK && master_complete_cb) master_complete_cb(TWI_OK, g_twi_txn.rx_buf, g_twi_txn.rx_idx);
        else if (error_cb) error_cb(res);
    }
}

TWI_Status_t twi_master_start_write(uint8_t slave_address, const uint8_t* data, size_t length) {
    if (g_twi_txn.state != TWI_STATE_IDLE) return TWI_ERROR_BUS_BUSY;

    g_twi_txn.state = TWI_STATE_START;
    g_twi_txn.slave_addr = slave_address;
    g_twi_txn.tx_buf = data;
    g_twi_txn.tx_len = length;
    g_twi_txn.tx_idx = 0;
    g_twi_txn.rx_len = 0;

    // Inicia START condition
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN) | (1 << TWIE);
    return TWI_OK;
}

// Master Start Read
TWI_Status_t twi_master_start_read(uint8_t slave_address, uint8_t* data, size_t length) {
    if (g_twi_txn.state != TWI_STATE_IDLE) return TWI_ERROR_BUS_BUSY;
    g_twi_txn.state = TWI_STATE_START;
    g_twi_txn.slave_addr = slave_address;
    g_twi_txn.tx_len = 0;
    g_twi_txn.rx_buf = data;
    g_twi_txn.rx_len = length;
    g_twi_txn.rx_idx = 0;
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN) | (1 << TWIE);
    return TWI_OK;
}

// Master Start Write-Read (combinada, com REPEATED START)
TWI_Status_t twi_master_start_write_read(uint8_t slave_address,
                                         const uint8_t* write_data, size_t write_length,
                                         uint8_t* read_data, size_t read_length) {
    if (g_twi_txn.state != TWI_STATE_IDLE) return TWI_ERROR_BUS_BUSY;

    g_twi_txn.state = TWI_STATE_START;
    g_twi_txn.slave_addr = slave_address;
    g_twi_txn.tx_buf = write_data; // Ponteiro para o buffer de escrita do usuário
    g_twi_txn.tx_len = write_length;
    g_twi_txn.tx_idx = 0;
    g_twi_txn.rx_buf = read_data;  // Ponteiro para onde o usuário quer salvar a leitura
    g_twi_txn.rx_len = read_length;
    g_twi_txn.rx_idx = 0;
    g_twi_txn.status = TWI_OK;

    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN) | (1 << TWIE);
    return TWI_OK;
}

// Função auxiliar para avançar FSM (se necessário, mas aqui tudo na ISR)
void twi_handle_state(void) {
    // Vazio por enquanto; lógica na ISR
}

TWI_Status_t twi_slave_process(void) {
    // Opcional: se precisar de polling híbrido, mas com interrupções puras, retorne TWI_OK
    return TWI_OK;
}

TWI_BusState_t twi_get_bus_status(void) {
    if (g_twi_txn.state == TWI_STATE_IDLE) {
        return TWI_BUS_IDLE;
    }
    
    if (g_twi_txn.state == TWI_STATE_ERROR || g_twi_txn.status != TWI_OK) {
        return TWI_BUS_ERROR;
    }
    
    return TWI_BUS_BUSY;
}

/**
 * @brief Inicia uma transação combinada de escrita seguida de leitura (Repeated Start).
 * 
 * Esta função configura o hardware para enviar uma sequência de bytes e, imediatamente 
 * após (sem liberar o barramento), inicia a leitura de uma quantidade específica de bytes.
 * É o método padrãoo para leitura de registradores em dispositivos como o DS3231.
 * 
 * @param slave_addr Endereço de 7 bits do dispositivo escravo (ex: 0x68).
 * @param tx_data    Ponteiro para o buffer de dados a serem enviados (ex: endereço do registrador).
 * @param tx_len     Quantidade de bytes a transmitir.
 * @param rx_data    Ponteiro para o buffer onde os dados recebidos serão armazenados.
 * @param rx_len     Quantidade de bytes a receber.
 * 
 * @return TWI_OK se a transação foi iniciada com sucesso.
 * @return TWI_ERROR_BUS_BUSY se o módulo TWI já estiver processando outra transação.
 */
TWI_Status_t twi_master_write_read(uint8_t slave_addr, const uint8_t* tx_data, size_t tx_len, uint8_t* rx_data, size_t rx_len) {
    // Verifica se a máquina de estados está ociosa. Se não estiver, evita sobreposição.
    if (g_twi_txn.state != TWI_STATE_IDLE) return TWI_ERROR_BUS_BUSY;

    // Configura os parâmetros da transação na estrutura global g_twi_txn
    g_twi_txn.state      = TWI_STATE_START; // Define que o próximo passo é enviar o START
    g_twi_txn.slave_addr = slave_addr;      // Armazena o endereço (será deslocado na ISR)
    
    // Configuração de Escrita
    g_twi_txn.tx_buf     = tx_data;         // Aponta para os dados de saída
    g_twi_txn.tx_len     = tx_len;          // Define quantos bytes enviar antes do Repeated Start
    g_twi_txn.tx_idx     = 0;               // Reseta o índice de transmissão
    
    // Configuração de Leitura
    g_twi_txn.rx_buf     = rx_data;         // Define o destino dos dados de entrada
    g_twi_txn.rx_len     = rx_len;          // Define quantos bytes esperar do escravo
    g_twi_txn.rx_idx     = 0;               // Reseta o índice de recepção
    
    g_twi_txn.status     = TWI_OK;          // Reseta status de erro anterior
    
    // Vincula o callback registrado pelo usuário é transação atual
    g_twi_txn.on_complete = master_complete_cb; 

    /* 
     * Disparo do Hardware:
     * TWINT: Limpa a flag de interrupção para iniciar a operação.
     * TWSTA: Gera a condição de START no barramento.
     * TWEN: Mantém o módulo TWI habilitado.
     * TWIE: Habilita a interrupção para que o restante da transação ocorra na ISR.
     */
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN) | (1 << TWIE);
    
    return TWI_OK;
}