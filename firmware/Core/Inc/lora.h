#pragma once

#include "stm32f0xx_hal.h"
#include <stdint.h>

/* ================= LoRa Enums ================= */

#define IRQ_TX_DONE        (1 << 0)
#define IRQ_RX_DONE        (1 << 1)
#define IRQ_TIMEOUT        (1 << 9)
#define IRQ_CRC_ERROR      (1 << 6)

#define LORA_RX_TIMEOUT_MS  5000

typedef struct {
    uint8_t length;
    int8_t  rssi;
    int8_t  snr;
} LoRaRxInfo_t;

typedef enum {
    LORA_EVENT_NONE       = 0,
    LORA_EVENT_TX_DONE    = 1 << 0,
    LORA_EVENT_RX_DONE    = 1 << 1,
    LORA_EVENT_TIMEOUT    = 1 << 2,
    LORA_EVENT_CRC_ERROR  = 1 << 3
} LoRaEvent_t;

typedef enum {
    LORA_MODE_SLEEP,
    LORA_MODE_STDBY,
    LORA_MODE_TX,
    LORA_MODE_RX
} LoRaMode_t;

typedef enum {
    LORA_SF5 = 5,
    LORA_SF6,
    LORA_SF7,
    LORA_SF8,
    LORA_SF9,
    LORA_SF10,
    LORA_SF11,
    LORA_SF12
} LoRaSpreadingFactor_t;

typedef enum {
    LORA_BW_7    = 0x00,
    LORA_BW_10   = 0x08,
    LORA_BW_15   = 0x01,
    LORA_BW_20   = 0x08,
    LORA_BW_31   = 0x02,
    LORA_BW_41   = 0x0A,
    LORA_BW_62   = 0x03,  
    LORA_BW_125  = 0x04,
    LORA_BW_250  = 0x05,
    LORA_BW_500  = 0x06
} LoRaBandwidth_t;

typedef enum {
    LORA_CR_4_5 = 0x01,
    LORA_CR_4_6,
    LORA_CR_4_7,
    LORA_CR_4_8
} LoRaCodingRate_t;

typedef enum {
    LORA_TX_POWER_M9_DBM = -0x09,
    LORA_TX_POWER_M8_DBM = -0x08,
    LORA_TX_POWER_M7_DBM = -0x07,
    LORA_TX_POWER_M6_DBM = -0x06,
    LORA_TX_POWER_M5_DBM = -0x05,
    LORA_TX_POWER_M4_DBM = -0x04,
    LORA_TX_POWER_M3_DBM = -0x03,
    LORA_TX_POWER_M2_DBM = -0x02,
    LORA_TX_POWER_M1_DBM = -0x01,
    LORA_TX_POWER_0_DBM = 0x00,
    LORA_TX_POWER_1_DBM,
    LORA_TX_POWER_2_DBM,
    LORA_TX_POWER_3_DBM,
    LORA_TX_POWER_4_DBM,
    LORA_TX_POWER_5_DBM,
    LORA_TX_POWER_6_DBM,
    LORA_TX_POWER_7_DBM,
    LORA_TX_POWER_8_DBM,
    LORA_TX_POWER_9_DBM,
    LORA_TX_POWER_10_DBM,
    LORA_TX_POWER_11_DBM,
    LORA_TX_POWER_12_DBM,
    LORA_TX_POWER_13_DBM,
    LORA_TX_POWER_14_DBM,
    LORA_TX_POWER_15_DBM,
    LORA_TX_POWER_16_DBM,
    LORA_TX_POWER_17_DBM,
    LORA_TX_POWER_18_DBM,
    LORA_TX_POWER_19_DBM,
    LORA_TX_POWER_20_DBM,
    LORA_TX_POWER_21_DBM,
    LORA_TX_POWER_22_DBM,
} LoRaTxPower_t;

/* ================= User Struct ================= */

typedef struct
{
    SPI_HandleTypeDef *hspi;

    GPIO_TypeDef *nss_port;
    uint16_t      nss_pin;

    GPIO_TypeDef *busy_port;
    uint16_t      busy_pin;

    GPIO_TypeDef *reset_port;
    uint16_t      reset_pin;

    GPIO_TypeDef *dio1_port;
    uint16_t      dio1_pin;

    uint32_t frequency;

    LoRaSpreadingFactor_t spreadingFactor;
    LoRaBandwidth_t       bandwidth;
    LoRaCodingRate_t      codingRate;

    uint16_t preambleLength;
    uint8_t  payloadLength;
    uint8_t  crcEnabled;

    LoRaTxPower_t txPower;

    LoRaMode_t currentMode;
    LoRaEvent_t events;
    volatile uint8_t irqPending;

} LoRa_t;

/* ================= API ================= */

void LoRa_Init(LoRa_t *lora);
void LoRa_Reset(LoRa_t *lora);

void LoRa_SetFrequency(LoRa_t *lora, uint32_t freq_hz);
void LoRa_Configure(LoRa_t *lora);

void LoRa_Transmit(LoRa_t *lora, uint8_t *data, uint8_t length);
void LoRa_Receive(LoRa_t *lora, uint32_t timeout_ms);
uint8_t LoRa_ReadPacket(LoRa_t *l, uint8_t *buffer, uint8_t maxLen, LoRaRxInfo_t *info);

void LoRa_IrqHandler(LoRa_t *lora);

