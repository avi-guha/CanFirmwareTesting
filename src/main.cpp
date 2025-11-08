/*
 * CAN Bus Sender - "Hello World"
 * Sends "hello world" message to another microcontroller via CAN bus
 * Upload this code to Microcontroller #1
 */

#include <Arduino.h>
#include <SPI.h>
#include <mcp2515.h>

// MCP2515 Pinout for ESP32 Pico Kit v4.1
// CS   -> GPIO 5
// MOSI -> GPIO 23 (default SPI)
// MISO -> GPIO 19 (default SPI)
// SCK  -> GPIO 18 (default SPI)

#define CAN_CS_PIN 5

MCP2515 mcp2515(CAN_CS_PIN);

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // Wait for serial port to connect
  }
  
  delay(1000);
  Serial.println("\n=== CAN Bus Sender ===");
  Serial.println("Sending 'hello world' message");
  
  // Initialize SPI
  SPI.begin();
  
  // Initialize MCP2515
  mcp2515.reset();
  
  // Set CAN bit rate to 500kbps with 16MHz crystal
  MCP2515::ERROR result = mcp2515.setBitrate(CAN_500KBPS, MCP_16MHZ);
  
  if (result == MCP2515::ERROR_OK) {
    Serial.println("✓ Bitrate set to 500kbps @ 16MHz");
  } else {
    Serial.println("✗ Error setting bitrate");
  }
  
  // Set to normal mode
  result = mcp2515.setNormalMode();
  
  if (result == MCP2515::ERROR_OK) {
    Serial.println("✓ MCP2515 initialized successfully!");
  } else {
    Serial.println("✗ Error: Check wiring!");
  }
  
  Serial.println("\nReady to send messages...");
  delay(1000);
}

void loop() {
  // Create CAN frame
  struct can_frame txFrame;
  txFrame.can_id = 0x100;  // CAN ID for "hello world" messages
  txFrame.can_dlc = 8;     // Data length: max 8 bytes for CAN 2.0
  
  // Fill frame with "hello wo" (first 8 chars of "hello world")
  // Note: Standard CAN frames can only hold 8 bytes max
  const char* message = "hello wo";
  for (int i = 0; i < 8; i++) {
    txFrame.data[i] = message[i];
  }
  
  // Send the message
  MCP2515::ERROR result = mcp2515.sendMessage(&txFrame);
  
  if (result == MCP2515::ERROR_OK) {
    Serial.print("✓ Sent: ");
    for (int i = 0; i < txFrame.can_dlc; i++) {
      Serial.print((char)txFrame.data[i]);
    }
    Serial.println();
  } else {
    Serial.print("✗ Send failed: ");
    switch(result) {
      case MCP2515::ERROR_ALLTXBUSY:
        Serial.println("All TX buffers busy");
        break;
      case MCP2515::ERROR_FAILINIT:
        Serial.println("Initialization failed");
        break;
      case MCP2515::ERROR_FAILTX:
        Serial.println("Transmission failed");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }
  
  // Send every 2 seconds
  delay(2000);
}
