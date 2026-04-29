# Driver TWI (I2C) para AVR (ATmega328P)

![License](https://img.shields.io/badge/license-MIT-green)
![AVR](https://img.shields.io/badge/MCU-AVR-blue)
![Language](https://img.shields.io/badge/language-C-darkblue)
![Compiler](https://img.shields.io/badge/compiler-XC8-orange)

Biblioteca robusta e otimizada para comunicação I2C/TWI, focada em performance e baixo consumo de CPU através de uma máquina de estados (FSM) totalmente baseada em interrupções.

---

## Diferenciais Técnicos

*   **Arquitetura Assíncrona:** O processador não fica "preso" esperando o barramento. As transações ocorrem via interrupção (ISR).
*   **Modo Híbrido:** Suporte completo para **Master** e **Slave** no mesmo driver.
*   **Event-Driven:** Utiliza callbacks para notificar a aplicação sobre o sucesso ou erro das operações.
*   **Repeated Start:** Implementação nativa de transações combinadas (Write-then-Read), essencial para sensores modernos.
*   **Segurança (Timeout):** Sistema de proteção contra travamentos do barramento (Bus Hang) com temporização configurável.
*   **Documentação Doxygen:** Código 100% comentado para geração automática de manuais técnicos.

---

## Conexão de Hardware (Pinagem)

Para o **ATmega328P** (Arduino Uno/Nano/Pro Mini), utilize as seguintes conexões:


| Pino TWI | Pino AVR | Descrição |
| :--- | :--- | :--- |
| **SDA** | **PC4 (A4)** | Serial Data Line |
| **SCL** | **PC5 (A5)** | Serial Clock Line |
| **GND** | **GND** | Referência Comum |

> **Nota:** Lembre-se de utilizar resistores de *pull-up* (4.7kΩ a 10kΩ) nas linhas SDA e SCL, já que o barramento é do tipo dreno-aberto.

---

## Exemplos Inclusos

O repositório conta com 3 demonstrações completas:

1.  **Master Simples:** Escrita e leitura básica de dados em um endereço específico.
2.  **Slave Completo:** Configuração do AVR para responder como um dispositivo periférico, reagindo a comandos do Master.
3.  **Sensor MPU6050:** Exemplo real de leitura de registradores internos (WHO_AM_I) utilizando *Repeated Start*.

---

## Como Usar

### 1. Inicialização
Configure o modo e a velocidade desejada no seu `main.c`:

<pre><code class="language-c">
sei(); // Habilita interrupções globais
twi_init(TWI_MODE_MASTER, 0, TWI_CLOCK_100KHZ);
</code></pre>

### 2. Registro de Callbacks
Defina a função que será chamada quando a comunicação terminar:

<pre><code class="language-c">
void ao_finalizar(TWI_Status_t status, const uint8_t* data, size_t len)
{
    if (status == TWI_OK)
    {
        // Sucesso! Trate os dados aqui.
    }
}

twi_master_register_callbacks(ao_finalizar, NULL);
</code></pre>

### 3. Disparar Transação
Inicie a comunicação. O código continuará rodando enquanto o hardware TWI trabalha em segundo plano:

<pre><code class="language-c">
uint8_t comando = 0x75;
twi_master_start_write_read(0x68, &comando, 1, buffer_leitura, 1);
</code></pre>

---

## Licença

Este projeto está sob a licença MIT - veja o arquivo [LICENSE](LICENSE) para detalhes.
