/**
 * @file twi.h
 * @brief Interface pública do driver TWI/I2C para AVR.
 * @details Proporciona funções para operação Master e Slave, suportando modos
 * de leitura, escrita e transações combinadas de forma assíncrona.
 */

#ifndef TWI_H
#define TWI_H

#ifndef F_CPU
/** @brief Define a frequência da CPU caso não tenha sido definida via compilador. */
#define F_CPU 16000000UL
#endif

#include "twi_types.h"
#include "twi_config.h"
#include <stddef.h>
/**
 * @brief Inicializa o módulo TWI.
 * @param mode Modo de operação (@ref TWI_MODE_MASTER ou @ref TWI_MODE_SLAVE).
 * @param address Endereço Slave (0-127). Ignorado no modo Master.
 * @param clock Frequência do barramento (@ref TWI_Clock_t).
 * @return TWI_Status_t Status da operação.
 */
TWI_Status_t twi_init(TWI_Mode_t mode, uint8_t address, TWI_Clock_t clock);

/**
 * @brief Desinicializa o módulo TWI, desligando o hardware e interrupções.
 */
void twi_deinit(void);

/**
 * @brief Verifica se o barramento TWI está disponível para uma nova transação.
 * @return 1 se livre, 0 se ocupado.
 */
uint8_t twi_bus_is_free(void);

/* --- Funções para Modo Master --- */

/**
 * @brief Inicia envio assíncrono de dados como Master.
 * @note Operação não-bloqueante. O callback de conclusão será chamado na ISR.
 * @param slave_address Endereço de 7 bits do dispositivo destino.
 * @param data Ponteiro para o buffer de dados a serem enviados.
 * @param length Quantidade de bytes no buffer.
 * @return TWI_OK se iniciado com sucesso, TWI_ERROR_BUS_BUSY se o barramento estiver ocupado.
 */
TWI_Status_t twi_master_start_write(uint8_t slave_address, const uint8_t* data, size_t length);

/**
 * @brief Inicia leitura assíncrona de dados como Master.
 * @param slave_address Endereço do dispositivo Slave.
 * @param data Buffer onde os dados lidos serão armazenados.
 * @param length Número de bytes a serem lidos.
 * @return TWI_Status_t TWI_OK se a leitura foi disparada.
 */
TWI_Status_t twi_master_start_read(uint8_t slave_address, uint8_t* data, size_t length);

/**
 * @brief Inicia uma transação combinada (Escrita seguida de Leitura) com Repeated Start.
 * @param slave_address Endereço do Slave.
 * @param write_data Buffer com dados de escrita (ex: endereço de registrador).
 * @param write_length Tamanho da escrita.
 * @param read_data Buffer para armazenar o retorno do Slave.
 * @param read_length Tamanho da leitura esperada.
 * @return TWI_Status_t Status de início da transação.
 */
TWI_Status_t twi_master_start_write_read(uint8_t slave_address, const uint8_t* write_data, size_t write_length,
                                         uint8_t* read_data, size_t read_length);

/**
 * @brief Registra os callbacks de eventos para o modo Master.
 * @param on_complete Função chamada após sucesso.
 * @param on_error Função chamada em caso de falha ou NACK.
 */
void twi_master_register_callbacks(TWI_Master_Complete_Callback_t on_complete, TWI_Error_Callback_t on_error);

/* --- Funções para Modo Slave --- */

/**
 * @brief Registra os callbacks de eventos para o modo Slave.
 * @param on_receive Chamado quando o Master envia dados para este endereço.
 * @param on_transmit Chamado quando o Master solicita dados deste endereço.
 */
void twi_slave_register_callbacks(TWI_Slave_Receive_Callback_t on_receive, TWI_Slave_Transmit_Callback_t on_transmit);

/**
 * @brief Processa eventos pendentes no Slave.
 * @note Deve ser chamada periodicamente no loop principal se não houver processamento direto na ISR.
 * @return TWI_Status_t Status do processamento.
 */
TWI_Status_t twi_slave_process(void);

/**
 * @brief Realiza um "ping" no endereço para verificar se o Slave está presente.
 * @param slave_address Endereço a ser testado.
 * @return TWI_Status_t TWI_OK se o comando de START foi enviado.
 */
TWI_Status_t twi_master_start_probe(uint8_t slave_address);

/**
 * @brief Verifica se a última transação foi abortada por tempo limite excedido.
 * @return 1 se ocorreu timeout, 0 caso contrário.
 */
uint8_t twi_had_timeout(void);

/**
 * @brief Estados lógicos do barramento TWI.
 */
typedef enum {
    TWI_BUS_IDLE,      /**< Barramento livre. */
    TWI_BUS_BUSY,      /**< Transação ativa em andamento. */
    TWI_BUS_ERROR      /**< Estado de erro detectado. */
} TWI_BusState_t;

/**
 * @brief Retorna o estado atual do barramento.
 * @return TWI_BusState_t Estado atual.
 */
TWI_BusState_t twi_get_bus_status(void);

/**
 * @brief Versão síncrona/bloqueante de escrita seguida de leitura.
 * @warning Pode bloquear a execução se não houver timeout adequado.
 */
TWI_Status_t twi_master_write_read(uint8_t slave_addr, const uint8_t* tx_data, size_t tx_len, uint8_t* rx_data, size_t rx_len);

#endif // TWI_H