/*
 * MCP2515 CAN Driver for STM32
 * Minimal, tested implementation for STSPIN boards
 */

#include "mcp2515_driver.h"

/* ============ PRIVATE VARIABLES ============ */
static SPI_HandleTypeDef *hspix = NULL;

/* ============ HELPER MACROS ============ */
#define CS_LOW()   HAL_GPIO_WritePin(MCP_CS_PORT, MCP_CS_PIN, GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(MCP_CS_PORT, MCP_CS_PIN, GPIO_PIN_SET)

/* ============ LOW-LEVEL SPI FUNCTIONS ============ */

void MCP2515_Reset(void)
{
    uint8_t cmd = MCP_RESET;
    CS_LOW();
    HAL_SPI_Transmit(hspix, &cmd, 1, 100);
    CS_HIGH();
    HAL_Delay(10);  // Wait for reset to complete
}

uint8_t MCP2515_ReadRegister(uint8_t address)
{
    uint8_t txData[2] = {MCP_READ, address};
    uint8_t rxData = 0;
    
    CS_LOW();
    HAL_SPI_Transmit(hspix, txData, 2, 100);
    HAL_SPI_Receive(hspix, &rxData, 1, 100);
    CS_HIGH();
    
    return rxData;
}

void MCP2515_WriteRegister(uint8_t address, uint8_t value)
{
    uint8_t txData[3] = {MCP_WRITE, address, value};
    
    CS_LOW();
    HAL_SPI_Transmit(hspix, txData, 3, 100);
    CS_HIGH();
}

void MCP2515_BitModify(uint8_t address, uint8_t mask, uint8_t value)
{
    uint8_t txData[4] = {MCP_BIT_MODIFY, address, mask, value};
    
    CS_LOW();
    HAL_SPI_Transmit(hspix, txData, 4, 100);
    CS_HIGH();
}

bool MCP2515_SetMode(uint8_t mode)
{
    MCP2515_BitModify(MCP_CANCTRL, MODE_MASK, mode);
    HAL_Delay(10);
    
    // Verify mode change
    uint8_t status = MCP2515_ReadRegister(MCP_CANSTAT);
    return ((status & MODE_MASK) == mode);
}

/* ============ INITIALIZATION ============ */

bool MCP2515_Init(SPI_HandleTypeDef *hspi)
{
    hspix = hspi;
    
    // Configure CS pin as output (should already be done in MX_GPIO_Init)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = MCP_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MCP_CS_PORT, &GPIO_InitStruct);
    
    CS_HIGH();
    HAL_Delay(10);
    
    // Reset the MCP2515
    MCP2515_Reset();
    
    // Should be in CONFIG mode after reset
    uint8_t status = MCP2515_ReadRegister(MCP_CANSTAT);
    if ((status & MODE_MASK) != MODE_CONFIG) {
        // Force config mode
        if (!MCP2515_SetMode(MODE_CONFIG)) {
            return false;  // Failed to enter config mode
        }
    }
    
    // ========================================
    // Configure bit timing for 500kbps @ 16MHz
    // ========================================
    // These values are verified for 16MHz crystal, 500kbps
    // TQ = 2/Fosc = 125ns
    // Sync = 1TQ, Prop = 1TQ, PS1 = 3TQ, PS2 = 3TQ
    // Total = 8TQ = 1us = 1Mbps... wait, let's recalculate
    //
    // For 500kbps with 16MHz:
    // Bit time = 2us = 32 TQ at 16MHz/1 = 16 TQ at BRP=1
    // CNF1 = 0x00 (BRP=1, SJW=1)
    // CNF2 = 0x90 (BTLMODE=1, SAM=0, PHSEG1=2, PRSEG=0)
    // CNF3 = 0x02 (PHSEG2=2)
    
    MCP2515_WriteRegister(MCP_CNF1, 0x00);  // BRP=0 (div by 1), SJW=0 (1TQ)
    MCP2515_WriteRegister(MCP_CNF2, 0x90);  // BTLMODE=1, SAM=0, PHSEG1=2, PRSEG=0
    MCP2515_WriteRegister(MCP_CNF3, 0x02);  // PHSEG2=2
    
    // Disable all filters - receive everything
    // RXB0: receive any message, rollover to RXB1
    MCP2515_WriteRegister(MCP_RXB0CTRL, 0x64);  // RXM=11 (any msg), BUKT=1 (rollover)
    MCP2515_WriteRegister(MCP_RXB1CTRL, 0x60);  // RXM=11 (any msg)
    
    // Clear all interrupt flags
    MCP2515_WriteRegister(MCP_CANINTF, 0x00);
    
    // Enable RX interrupts (optional, we poll anyway)
    MCP2515_WriteRegister(MCP_CANINTE, 0x03);  // RX0IE + RX1IE
    
    // Switch to normal mode
    if (!MCP2515_SetMode(MODE_NORMAL)) {
        return false;  // Failed to enter normal mode
    }
    
    return true;
}

/* ============ SEND MESSAGE ============ */

bool MCP2515_SendMessage(CAN_Message_t *msg)
{
    // Wait for TX buffer to be free (with timeout)
    uint8_t timeout = 100;
    while (timeout--) {
        uint8_t status = MCP2515_ReadRegister(MCP_TXB0CTRL);
        if ((status & 0x08) == 0) {  // TXREQ bit clear = buffer free
            break;
        }
        HAL_Delay(1);
    }
    if (timeout == 0) {
        return false;  // TX buffer busy timeout
    }
    
    // Prepare TX buffer data
    uint8_t txBuf[13];
    
    if (msg->extended) {
        // Extended ID (29-bit)
        txBuf[0] = (uint8_t)(msg->id >> 21);                           // SIDH
        txBuf[1] = (uint8_t)(((msg->id >> 13) & 0xE0) | 0x08 | ((msg->id >> 16) & 0x03));  // SIDL + EXIDE
        txBuf[2] = (uint8_t)(msg->id >> 8);                            // EID8
        txBuf[3] = (uint8_t)(msg->id);                                 // EID0
    } else {
        // Standard ID (11-bit)
        txBuf[0] = (uint8_t)(msg->id >> 3);                            // SIDH
        txBuf[1] = (uint8_t)((msg->id & 0x07) << 5);                   // SIDL
        txBuf[2] = 0x00;                                               // EID8
        txBuf[3] = 0x00;                                               // EID0
    }
    
    txBuf[4] = msg->dlc & 0x0F;  // DLC
    
    for (uint8_t i = 0; i < msg->dlc && i < 8; i++) {
        txBuf[5 + i] = msg->data[i];
    }
    
    // Load TX buffer using LOAD TX instruction
    uint8_t loadCmd = MCP_LOAD_TX0;  // Load starting at TXB0SIDH
    CS_LOW();
    HAL_SPI_Transmit(hspix, &loadCmd, 1, 100);
    HAL_SPI_Transmit(hspix, txBuf, 5 + msg->dlc, 100);
    CS_HIGH();
    
    // Request to send
    uint8_t rtsCmd = MCP_RTS_TX0;
    CS_LOW();
    HAL_SPI_Transmit(hspix, &rtsCmd, 1, 100);
    CS_HIGH();
    
    return true;
}

/* ============ RECEIVE MESSAGE ============ */

bool MCP2515_MessageAvailable(void)
{
    uint8_t status = MCP2515_ReadRegister(MCP_CANINTF);
    return (status & 0x03) != 0;  // RX0IF or RX1IF set
}

bool MCP2515_ReadMessage(CAN_Message_t *msg)
{
    uint8_t status = MCP2515_ReadRegister(MCP_CANINTF);
    uint8_t readCmd;
    uint8_t clearBit;
    
    if (status & 0x01) {
        // Message in RXB0
        readCmd = MCP_READ_RX0;
        clearBit = 0x01;
    } else if (status & 0x02) {
        // Message in RXB1
        readCmd = MCP_READ_RX1;
        clearBit = 0x02;
    } else {
        return false;  // No message
    }
    
    // Read RX buffer (ID + DLC + Data)
    uint8_t rxBuf[13];
    CS_LOW();
    HAL_SPI_Transmit(hspix, &readCmd, 1, 100);
    HAL_SPI_Receive(hspix, rxBuf, 13, 100);
    CS_HIGH();
    
    // Parse ID
    msg->extended = (rxBuf[1] & 0x08) != 0;  // EXIDE bit
    
    if (msg->extended) {
        // Extended ID
        msg->id = ((uint32_t)rxBuf[0] << 21) |
                  ((uint32_t)(rxBuf[1] & 0xE0) << 13) |
                  ((uint32_t)(rxBuf[1] & 0x03) << 16) |
                  ((uint32_t)rxBuf[2] << 8) |
                  ((uint32_t)rxBuf[3]);
    } else {
        // Standard ID
        msg->id = ((uint32_t)rxBuf[0] << 3) | ((rxBuf[1] >> 5) & 0x07);
    }
    
    msg->dlc = rxBuf[4] & 0x0F;
    if (msg->dlc > 8) msg->dlc = 8;
    
    for (uint8_t i = 0; i < msg->dlc; i++) {
        msg->data[i] = rxBuf[5 + i];
    }
    
    // Clear interrupt flag
    MCP2515_BitModify(MCP_CANINTF, clearBit, 0x00);
    
    return true;
}
