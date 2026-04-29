/**
 * @file twi_types.h
 * @brief Definições de tipos, enumerações e assinaturas de callback do driver TWI.
 * @details Este arquivo centraliza os tipos de dados fundamentais para garantir a 
 * consistência entre os modos Master e Slave.
 */
#ifndef TWI_TYPES_H
#define TWI_TYPES_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Modos de operação do módulo TWI.
 */
typedef enum {
    TWI_MODE_MASTER,  /**< O microcontrolador controla o barramento e gera o clock. */
    TWI_MODE_SLAVE    /**< O microcontrolador responde a um endereço definido pelo Master. */
} TWI_Mode_t;

/**
 * @brief Frequências de clock (SCL) suportadas para o barramento.
 */
typedef enum {
    TWI_CLOCK_100KHZ,  /**< 100 kHz (Standard Mode). */
    TWI_CLOCK_400KHZ   /**< 400 kHz (Fast Mode). */
} TWI_Clock_t;

/**
 * @brief Códigos de status e erro para operações TWI.
 */
typedef enum {
    TWI_OK,                /**< Operação concluída com sucesso. */
    TWI_ERROR_NACK,        /**< Dispositivo não respondeu (Not Acknowledge). */
    TWI_ERROR_BUS_BUSY,    /**< O barramento já está sendo usado por outra transação. */
    TWI_ERROR_ARBIT_LOST,  /**< Perda de arbitragem (comum em sistemas Multi-Master). */
    TWI_ERROR_TIMEOUT,     /**< A operação excedeu o tempo limite de resposta. */
    TWI_ERROR_INVALID_ARG  /**< Parâmetro inválido passado para a função. */
} TWI_Status_t;

/**
 * @brief Callback para o modo Slave: acionado após o recebimento de dados.
 * @param data Ponteiro para o buffer contendo os dados recebidos.
 * @param length Quantidade de bytes recebidos do Master.
 */
typedef void (*TWI_Slave_Receive_Callback_t)(const uint8_t* data, size_t length);

/**
 * @brief Callback para o modo Slave: acionado quando o Master solicita dados.
 * @param data Buffer que deve ser preenchido pela aplicação com os dados a enviar.
 * @param max_length Espaço total disponível no buffer de saída.
 * @return size_t Quantidade real de bytes que a aplicação colocou no buffer.
 */
typedef size_t (*TWI_Slave_Transmit_Callback_t)(uint8_t* data, size_t max_length);

/**
 * @brief Callback de conclusão para o modo Master.
 * @param status Status final da transação (@ref TWI_Status_t).
 * @param data Ponteiro para os dados recebidos (será NULL em operações apenas de escrita).
 * @param length Quantidade de bytes processados na transação.
 */
typedef void (*TWI_Master_Complete_Callback_t)(TWI_Status_t status, const uint8_t* data, size_t length);

/**
 * @brief Callback genérico para tratamento de erros no barramento.
 * @param error Código do erro detectado.
 */
typedef void (*TWI_Error_Callback_t)(TWI_Status_t error);  ///< Callback gen�rico para erros.

#endif // TWI_TYPES_H