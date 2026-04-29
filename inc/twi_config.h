/**
 * @file twi_config.h
 * @brief Configurações estáticas do driver TWI.
 * @details Permite ajustar o consumo de memória, recursos de depuração e 
 * comportamento padrão do hardware antes da compilação.
 */
#ifndef TWI_CONFIG_H
#define TWI_CONFIG_H

/** 
 * @name Configurações de Depuração
 * @{ 
 */

/**
 * @brief Define o modo de operação da biblioteca.
 * @details 
 * - **0 (Modo LIGHT):** Recomendado para produção. Sem logs ou variáveis de debug, 
 *   priorizando performance e economia de memória Flash/RAM.
 * - **1 (Modo DEBUG):** Recomendado para desenvolvimento. Habilita contadores de interrupção, 
 *   rastreio do último status do barramento e variáveis de monitoramento da ISR.
 */
#define TWI_DEBUG_MODE          0  
/** @} */

/** 
 * @name Parâmetros do Barramento
 * @{ 
 */

/** 
 * @brief Frequência de clock padrão inicial do barramento.
 * @see TWI_Clock_t 
 */
#define TWI_DEFAULT_CLOCK       TWI_CLOCK_100KHZ

/** 
 * @brief Habilita a resposta ao endereço de chamada geral (0x00).
 * @details 1 para habilitar, 0 para ignorar chamadas gerais no modo Slave. 
 */
#define TWI_ENABLE_GENERAL_CALL 1
/** @} */

/** 
 * @name Dimensionamento de Memória
 * @{ 
 */

/**
 * @brief Tamanho dos buffers internos de recepção e transmissão do Slave.
 * @warning O valor deve ser ajustado com base na memória RAM disponível no MCU (ex: ATmega328p vs ATtiny).
 * @note Este valor define o limite máximo de bytes por transação no modo Slave.
 */
#define TWI_RX_BUFFER_SIZE      32
/** @} */

#endif // TWI_CONFIG_H