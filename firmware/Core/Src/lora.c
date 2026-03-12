#include "lora.h"
#include "usb.h"


//#define DEBUG
//#define DEBUG_LORA

#define dbg_print(x) \
          do { if (DEBUG) print(x); } while (0)



/* ================= SX126x Commands ================= */

#define CMD_SET_SLEEP               0x84
#define CMD_SET_STANDBY             0x80
#define CMD_SET_FS                  0xC1
#define CMD_SET_TX                  0x83
#define CMD_SET_RX                  0x82
#define CMD_WRITE_REGISTER          0x0D
#define CMD_READ_REGISTER           0x1D
#define CMD_WRITE_BUFFER            0x0E
#define CMD_READ_BUFFER             0x1E
#define CMD_SET_RF_FREQUENCY        0x86
#define CMD_SET_PACKET_PARAMS       0x8C
#define CMD_SET_MODULATION_PARAMS   0x8B
#define CMD_SET_TX_PARAMS           0x8E
#define CMD_SET_DIO_IRQ_PARAMS      0x08
#define CMD_CLEAR_IRQ_STATUS        0x02
#define CMD_SET_PACKET_TYPE         0x8A
#define CMD_SET_BUFFER_BASE_ADDRESS 0x8F

#define CMD_SET_DIO_IRQ_PARAMS      0x08
#define CMD_GET_IRQ_STATUS          0x12
#define CMD_CLEAR_IRQ_STATUS        0x02

#define CMD_GET_RX_BUFFER_STATUS    0x13
#define CMD_GET_PACKET_STATUS       0x14

/* ================= Helpers ================= */

static void NSS_Low(LoRa_t *l) {
    HAL_GPIO_WritePin(l->nss_port, l->nss_pin, GPIO_PIN_RESET);
}

static void NSS_High(LoRa_t *l) {
    HAL_GPIO_WritePin(l->nss_port, l->nss_pin, GPIO_PIN_SET);
}

static void WaitBusy(LoRa_t *l) {
    while (HAL_GPIO_ReadPin(l->busy_port, l->busy_pin) == GPIO_PIN_SET);
}

static void SpiCmd(LoRa_t *l, uint8_t *tx, uint8_t *rx, uint16_t len) {
    WaitBusy(l);
    NSS_Low(l);
    HAL_SPI_TransmitReceive(l->hspi, tx, rx, len, HAL_MAX_DELAY);
    NSS_High(l);
    WaitBusy(l);
    
#ifdef DEBUG_LORA
    print("CMDW\t");
    print_bytes(tx, len);
    print("\nRX\t");
    print_bytes(rx, len);
    print("\n");
#endif
}

static void LoRa_ClearIrq(LoRa_t *l, uint16_t mask)
{
    uint8_t cmd[] = {
        CMD_CLEAR_IRQ_STATUS,
        (mask >> 8) & 0xFF,
        mask & 0xFF
    };
    uint8_t rx[sizeof(cmd)];
    SpiCmd(l, cmd, rx, sizeof(cmd));
}

static void LoRa_SetIrqConfig(LoRa_t *l)
{
    uint16_t irqMask = IRQ_TX_DONE | IRQ_RX_DONE | IRQ_TIMEOUT;

    uint8_t cmd[] = {
        CMD_SET_DIO_IRQ_PARAMS,
        (irqMask >> 8) & 0xFF,    // IRQ mask MSB
        irqMask & 0xFF,           // IRQ mask LSB

        (irqMask >> 8) & 0xFF,    // DIO1 mask MSB
        irqMask & 0xFF,           // DIO1 mask LSB

        0x00, 0x00,               // DIO2 mask
        0x00, 0x00                // DIO3 mask
    };

    uint8_t rx[sizeof(cmd)];
    SpiCmd(l, cmd, rx, sizeof(cmd));
}

static void LoRa_SetBufferOffsets(LoRa_t *l, uint8_t tx_offset, uint8_t rx_offset) {
  uint8_t cmd[] = {
    CMD_SET_BUFFER_BASE_ADDRESS,
    tx_offset,
    rx_offset,
  };

  uint8_t rx[sizeof(cmd)];

  SpiCmd(l, cmd, rx, sizeof(cmd));

}

static uint8_t LoRa_ReadRegister(LoRa_t *l, uint16_t address) {
  uint8_t cmd[] = {
    CMD_READ_REGISTER,
    (address >> 8) & 0xFF,
    address & 0xFF,
    0x00, 0x00
  };

  uint8_t rx[sizeof(cmd)];

  SpiCmd(l, cmd, rx, sizeof(cmd));

  return rx[sizeof(cmd)];
}

static void LoRa_WriteRegister(LoRa_t *l, uint16_t address, uint8_t data) {
  uint8_t cmd[] = {
    CMD_WRITE_REGISTER,
    (address >> 8) & 0xFF,
    address & 0xFF,
    data
  };

  uint8_t rx[sizeof(cmd)];

  SpiCmd(l, cmd, rx, sizeof(cmd));
}

/* ================= Core ================= */

void LoRa_Reset(LoRa_t *l)
{
    HAL_GPIO_WritePin(l->reset_port, l->reset_pin, GPIO_PIN_RESET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(l->reset_port, l->reset_pin, GPIO_PIN_SET);
    HAL_Delay(10);
}

void LoRa_Init(LoRa_t *l)
{
    LoRa_Reset(l);

    uint8_t cmd[] = { CMD_SET_STANDBY, 0x00 };
    uint8_t rx[2];
    SpiCmd(l, cmd, rx, sizeof(cmd));

    LoRa_SetFrequency(l, l->frequency);
    LoRa_Configure(l);
    LoRa_SetIrqConfig(l);

    l->currentMode = LORA_MODE_STDBY;
}

void LoRa_SetFrequency(LoRa_t *l, uint32_t freq_hz)
{
    uint64_t frf = ((uint64_t)freq_hz << 25) / 32000000UL;
    uint8_t cmd[5] = {
        CMD_SET_RF_FREQUENCY,
        (frf >> 24) & 0xFF,
        (frf >> 16) & 0xFF,
        (frf >> 8)  & 0xFF,
        frf & 0xFF
    };
    uint8_t rx[5];
    SpiCmd(l, cmd, rx, sizeof(cmd));
}

void LoRa_Configure(LoRa_t *l)
{   
    uint8_t type[2] = {
      CMD_SET_PACKET_TYPE,
      1
    };
    
    uint8_t rx0[2];
    SpiCmd(l, type, rx0, sizeof(type));
    
    uint8_t mod[5] = {
        CMD_SET_MODULATION_PARAMS,
        l->spreadingFactor,
        l->bandwidth,
        l->codingRate,
        0x00
    };
     
    uint8_t rx1[5];
    SpiCmd(l, mod, rx1, sizeof(mod));

    uint8_t pkt[7] = {
        CMD_SET_PACKET_PARAMS,
        (l->preambleLength >> 8) & 0xFF,
        l->preambleLength & 0xFF,
        0x00,
        l->payloadLength,
        l->crcEnabled ? 0x01 : 0x00,
        0x00
    };

    uint8_t rx2[7];
    SpiCmd(l, pkt, rx2, sizeof(pkt));

    LoRa_SetBufferOffsets(l, 0x00, 0x00);
    
}

void LoRa_ApplyConfig(LoRa_t *l, uint32_t freq, uint8_t sf, uint8_t bw)
{
    l->frequency = freq;
    l->spreadingFactor = sf;
    l->bandwidth = bw;
    LoRa_SetFrequency(l, freq);
    LoRa_Configure(l);
}

void LoRa_Transmit(LoRa_t *l, uint8_t *data, uint8_t len)
{
    uint8_t pkt[7] = {
        CMD_SET_PACKET_PARAMS,
        (l->preambleLength >> 8) & 0xFF,
        l->preambleLength & 0xFF,
        0x00,          // explicit header
        len,           // <-- actual TX length
        l->crcEnabled ? 0x01 : 0x00,
        0x00
    };

    uint8_t rx_pkt[7];
    SpiCmd(l, pkt, rx_pkt, sizeof(pkt));
    
    uint8_t buf[1 + 1 + 255];
    buf[0] = CMD_WRITE_BUFFER;
    buf[1] = 0x00;
    memcpy(&buf[2], data, len);


    uint8_t rx[sizeof(buf)];
    SpiCmd(l, buf, rx, len + 2);


    uint8_t tx_cmd[] = { CMD_SET_TX, 0x00, 0x00, 0x00 };
    uint8_t rx2[4];
    
    LoRa_ClearIrq(l, 0xFFFF);
    
    SpiCmd(l, tx_cmd, rx2, sizeof(tx_cmd));

    l->currentMode = LORA_MODE_TX;
}

void LoRa_Receive(LoRa_t *l, uint32_t timeout_ms)
{
    uint32_t timeout = timeout_ms * 64;
    if (timeout > 0x00FFFFFF)
        timeout = 0x00FFFFFF;

    uint8_t rx_cmd[] = {
        CMD_SET_RX,
        (timeout >> 16) & 0xFF,
        (timeout >> 8)  & 0xFF,
        timeout & 0xFF
    };

    uint8_t rx[4];
    
    LoRa_ClearIrq(l, 0xFFFF);
    
    SpiCmd(l, rx_cmd, rx, sizeof(rx_cmd));

    l->currentMode = LORA_MODE_RX;
}

uint8_t LoRa_ReadPacket(LoRa_t *l, uint8_t *buffer, uint8_t maxLen, LoRaRxInfo_t *info)
{
    uint8_t cmd[4];
    uint8_t rx[4];

    /* --- Get RX buffer status --- */
    cmd[0] = CMD_GET_RX_BUFFER_STATUS;
    cmd[1] = 0x00;
    cmd[2] = 0x00;
    cmd[3] = 0x00;

    SpiCmd(l, cmd, rx, sizeof(cmd));

    uint8_t payloadLen = rx[2];
    uint8_t startPtr   = rx[3];

    if (payloadLen > maxLen)
        payloadLen = maxLen;

    /* --- Read RX buffer --- */
    uint8_t readCmd[3] = {
        CMD_READ_BUFFER,
        startPtr,
        0
    };

    WaitBusy(l);
    NSS_Low(l);
    HAL_SPI_Transmit(l->hspi, readCmd, sizeof(readCmd), HAL_MAX_DELAY);
    HAL_SPI_Receive(l->hspi, buffer, payloadLen, HAL_MAX_DELAY);
    NSS_High(l);
    WaitBusy(l);

#ifdef DEBUG_LORA
    print("CMDR\t");
    print_bytes(readCmd, sizeof(readCmd));
    print("\nRX\t");
    print_bytes(buffer, payloadLen);
    print("\n");
#endif

    /* --- Optional: get RSSI / SNR --- */
    if (info) {
      uint8_t psCmd[4] = { CMD_GET_PACKET_STATUS, 0x00, 0x00, 0x00 };
        uint8_t psRx[4];

        SpiCmd(l, psCmd, psRx, sizeof(psCmd));

        info->rssi = -(int8_t)(psRx[2] >> 1);
        info->snr  =  (int8_t)(psRx[3] >> 2);
        info->length = payloadLen;
#ifdef DEBUG_LORA      
        print("RSSI\t");
        print_byte(info->rssi);
        print("\nSNR\t");
        print_byte(info->snr);
        print("\nLEN\t");
        print_byte(info->length);
        print("\n");
#endif
    }

    return payloadLen;
}


void LoRa_IrqHandler(LoRa_t *l)
{
    uint8_t cmd[4] = { CMD_GET_IRQ_STATUS, 0x00, 0x00, 0x00 };
    uint8_t rx[4];

    char* buf;
    SpiCmd(l, cmd, rx, sizeof(cmd));

    uint16_t irq = (rx[2] << 8) | rx[3];

    if (irq & IRQ_TX_DONE)
    {
        l->currentMode = LORA_MODE_STDBY;
        l->events |= LORA_EVENT_TX_DONE;
        buf = "TX";
    }

    if (irq & IRQ_RX_DONE)
    {
        l->currentMode = LORA_MODE_STDBY;
        l->events |= LORA_EVENT_RX_DONE;
        buf = "RX";
    }
    
    if (irq & IRQ_CRC_ERROR) {
      buf = "CRC";
    }

    if (irq & IRQ_TIMEOUT)
    {
        l->currentMode = LORA_MODE_STDBY;
        l->events |= LORA_EVENT_TIMEOUT;
        buf = "TO";
    }

#ifdef DEBUG_LORA
    print("IRQ(");
    print(buf);
    print(")\n");
#endif

    LoRa_ClearIrq(l, irq);
    l->irqPending = 1;
}
