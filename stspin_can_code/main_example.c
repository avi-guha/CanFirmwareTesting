/*
 * STSPIN CAN Example - Main Code
 * 
 * This code initializes the MCP2515 and:
 * 1. Sends a startup message (ID 0x555) on boot
 * 2. When any CAN message is received, responds with Extended ID 10101 + 0xDEADBEEF
 * 
 * HOW TO USE:
 * 1. Copy mcp2515_driver.h and mcp2515_driver.c to your STM32 project
 * 2. In mcp2515_driver.h, verify MCP_CS_PORT and MCP_CS_PIN match your wiring
 * 3. Add the code below to your main.c in the appropriate sections
 */

/* ============================================
   ADD TO INCLUDES (at top of main.c):
   ============================================ */
/*
#include "mcp2515_driver.h"
*/

/* ============================================
   ADD TO PRIVATE VARIABLES section:
   ============================================ */
/*
CAN_Message_t rxMsg;
CAN_Message_t txMsg;
*/

/* ============================================
   ADD TO USER CODE BEGIN 2 (before while loop):
   ============================================ */
/*
  // Initialize MCP2515 CAN controller
  if (MCP2515_Init(&hspi1)) {
      // Success! Send startup message
      txMsg.id = 0x555;
      txMsg.dlc = 2;
      txMsg.data[0] = 0x12;
      txMsg.data[1] = 0x34;
      txMsg.extended = false;
      MCP2515_SendMessage(&txMsg);
  }
*/

/* ============================================
   ADD INSIDE while(1) loop:
   ============================================ */
/*
    // Check for incoming CAN message
    if (MCP2515_MessageAvailable()) {
        if (MCP2515_ReadMessage(&rxMsg)) {
            // Message received! Send response with Extended ID 10101
            txMsg.id = 10101;          // Decimal 10101
            txMsg.dlc = 4;
            txMsg.data[0] = 0xDE;
            txMsg.data[1] = 0xAD;
            txMsg.data[2] = 0xBE;
            txMsg.data[3] = 0xEF;
            txMsg.extended = true;     // Extended (29-bit) ID
            MCP2515_SendMessage(&txMsg);
        }
    }
    
    HAL_Delay(10);  // Small delay
*/

/* ============================================
   COMPLETE EXAMPLE main.c (relevant parts):
   ============================================ */

#if 0  // Set to 1 to see the complete example

#include "main.h"
#include "mcp2515_driver.h"

SPI_HandleTypeDef hspi1;
CAN_Message_t rxMsg;
CAN_Message_t txMsg;

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();
    
    // Initialize MCP2515
    if (MCP2515_Init(&hspi1)) {
        // Send startup beacon - ESP32 should see this
        txMsg.id = 0x555;
        txMsg.dlc = 4;
        txMsg.data[0] = 'B';
        txMsg.data[1] = 'O';
        txMsg.data[2] = 'O';
        txMsg.data[3] = 'T';
        txMsg.extended = false;
        MCP2515_SendMessage(&txMsg);
    }
    
    while (1)
    {
        if (MCP2515_MessageAvailable()) {
            if (MCP2515_ReadMessage(&rxMsg)) {
                // Echo back with response
                txMsg.id = 10101;
                txMsg.dlc = 4;
                txMsg.data[0] = 0xDE;
                txMsg.data[1] = 0xAD;
                txMsg.data[2] = 0xBE;
                txMsg.data[3] = 0xEF;
                txMsg.extended = true;
                MCP2515_SendMessage(&txMsg);
            }
        }
        
        HAL_Delay(10);
    }
}

// SPI1 Init - adjust pins for your board
static void MX_SPI1_Init(void)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;  // Adjust for ~1MHz SPI
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    HAL_SPI_Init(&hspi1);
}

#endif
