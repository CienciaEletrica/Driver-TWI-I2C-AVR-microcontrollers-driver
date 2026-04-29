/**
 * @file twi_internal.h
 * @brief Definições internas e Máquina de Estados (FSM) do driver TWI.
 * @details Este cabeçalho contém as estruturas e estados que não devem ser 
 * acessados diretamente pelo usuário, mas que são fundamentais para o 
 * funcionamento da ISR (Interrupt Service Routine).
 */
#ifndef TWI_INTERNAL_H
#define TWI_INTERNAL_H

#include "twi_types.h"
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

/**
 * @brief Estados da Máquina de Estados Finita (FSM) do TWI.
 */
typedef enum {
    TWI_STATE_IDLE,     /**< Barramento ocioso, nenhuma transação ativa. */
    TWI_STATE_START,    /**< Condição de START enviada. */
    TWI_STATE_SLA_W,    /**< Endereço do Slave + bit de Escrita (W) enviado. */
    TWI_STATE_SLA_R,    /**< Endereço do Slave + bit de Leitura (R) enviado. */
    TWI_STATE_DATA_TX,  /**< Transmitindo dados (Master ou Slave). */
    TWI_STATE_DATA_RX,  /**< Recebendo dados (Master ou Slave). */
    TWI_STATE_SLAVE_TX, /**< Slave em modo de transmissão ativa. */
    TWI_STATE_STOP,     /**< Condição de STOP sendo gerada. */
    TWI_STATE_ERROR,    /**< Estado de erro detectado. */
    TWI_STATE_PROBE     /**< Verificando presença de dispositivo no barramento. */
} TWI_State_t;

/**
 * @brief Estrutura de controle para a transação TWI ativa.
 * @details Armazena buffers, contadores e ponteiros de função para a 
 * operação que está sendo processada pela ISR no momento.
 */
typedef struct {
    volatile TWI_State_t state;     /**< Estado lógico atual da FSM. */
    volatile TWI_Status_t status;   /**< Status atual da transação. */
    uint8_t slave_addr;             /**< Endereço do dispositivo Slave alvo. */
    const uint8_t* tx_buf;          /**< Ponteiro para buffer de transmissão. */
    uint8_t* rx_buf;                /**< Ponteiro para buffer de recepção. */
    size_t tx_len;                  /**< Total de bytes a transmitir. */
    size_t rx_len;                  /**< Total de bytes a receber. */
    size_t tx_idx;                  /**< Índice atual de transmissão. */
    size_t rx_idx;                  /**< Índice atual de recepção. */
    
    /** @brief Callback executado após o sucesso da transação. */
    TWI_Master_Complete_Callback_t on_complete; 
    /** @brief Callback executado em caso de erro ou timeout. */
    TWI_Error_Callback_t on_error;
    
    uint16_t timeout_counter;       /**< Contador incremental de ticks da ISR. */
    uint16_t timeout_limit;         /**< Limite máximo de ticks antes do erro de timeout. */
} TWI_Transaction_t;

/** 
 * @brief Instância global da transação ativa.
 * @note Declarada como externa para que a ISR em twi.c possa manipulá-la.
 */
extern volatile TWI_Transaction_t g_twi_txn;  // Global para ISR acessar

/**
 * @brief Gerenciador principal da Máquina de Estados.
 * @details Chamado dentro da interrupção para decidir o próximo passo com base no status do hardware.
 */
void twi_handle_state(void);  // Avan�a a FSM na ISR

/* --- Variáveis de Depuração --- */
#if (TWI_DEBUG_MODE == 1)
    /** @brief Último código de status lido do registrador TWSR. */
    extern volatile uint8_t debug_twi_last_status;
    /** @brief Último estado lógico antes da interrupção atual. */
    extern volatile TWI_State_t debug_twi_last_state;
    /** @brief Contador global de disparos da interrupção. */
    extern volatile uint8_t debug_twi_counter;
#endif

#endif // TWI_INTERNAL_H