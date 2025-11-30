/*
 * MCP2515 CAN Driver for STM32
 * Minimal, tested implementation for STSPIN boards
 */

#ifndef MCP2515_DRIVER_H
#define MCP2515_DRIVER_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* ============ PIN CONFIGURATION ============ */
// Change these to match your wiring!
#define MCP_CS_PORT     GPIOA
#define MCP_CS_PIN      GPIO_PIN_15

/* ============ MCP2515 COMMANDS ============ */
#define MCP_RESET       0xC0
#define MCP_READ        0x03
#define MCP_WRITE       0x02
#define MCP_BIT_MODIFY  0x05
#define MCP_READ_STATUS 0xA0
#define MCP_RX_STATUS   0xB0
#define MCP_READ_RX0    0x90
#define MCP_READ_RX1    0x94
#define MCP_LOAD_TX0    0x40
#define MCP_RTS_TX0     0x81
#define MCP_RTS_TX1     0x82
#define MCP_RTS_TX2     0x84

/* ============ MCP2515 REGISTERS ============ */
#define MCP_CANSTAT     0x0E
#define MCP_CANCTRL     0x0F
#define MCP_CNF3        0x28
#define MCP_CNF2        0x29
#define MCP_CNF1        0x2A
#define MCP_CANINTE     0x2B
#define MCP_CANINTF     0x2C
#define MCP_TXB0CTRL    0x30
#define MCP_RXB0CTRL    0x60
#define MCP_RXB1CTRL    0x70

/* ============ MODE BITS ============ */
#define MODE_NORMAL     0x00
#define MODE_SLEEP      0x20
#define MODE_LOOPBACK   0x40
#define MODE_LISTENONLY 0x60
#define MODE_CONFIG     0x80
#define MODE_MASK       0xE0

/* ============ CAN MESSAGE STRUCTURE ============ */
typedef struct {
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[8];
    bool     extended;
} CAN_Message_t;

/* ============ FUNCTION PROTOTYPES ============ */

// Initialize MCP2515 - call this once at startup
// Returns true if successful
bool MCP2515_Init(SPI_HandleTypeDef *hspi);

// Send a CAN message
// Returns true if message was queued successfully
bool MCP2515_SendMessage(CAN_Message_t *msg);

// Check if a message is available
bool MCP2515_MessageAvailable(void);

// Read a received message
// Returns true if a message was read
bool MCP2515_ReadMessage(CAN_Message_t *msg);

// Low-level functions (usually not needed directly)
void MCP2515_Reset(void);
uint8_t MCP2515_ReadRegister(uint8_t address);
void MCP2515_WriteRegister(uint8_t address, uint8_t value);
void MCP2515_BitModify(uint8_t address, uint8_t mask, uint8_t value);
bool MCP2515_SetMode(uint8_t mode);

#endif // MCP2515_DRIVER_H
