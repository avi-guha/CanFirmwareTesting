#ifdef ROLE_SENDER
/*
 * CAN Bus Sender for STSPIN Motor Controller
 * 
 * Matches STSPIN MCP2515 configuration:
 * - 8MHz crystal, 500kbps CAN bus
 * - STSPIN accepts any CAN message and responds with Extended ID 10101
 * - Response data: {0xDE, 0xAD, 0xBE, 0xEF}
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

// STSPIN responds with Extended ID 10101 (decimal)
static const uint32_t STSPIN_RESPONSE_ID = 10101;

MCP2515 mcp2515(CAN_CS_PIN);

// Send a CAN frame with retry logic
static bool sendFrame(uint32_t canId, const uint8_t* data, uint8_t dlc, bool extended = false) {
  struct can_frame tx;
  
  if (extended) {
    tx.can_id = canId | CAN_EFF_FLAG;  // Extended Frame Format
  } else {
    tx.can_id = canId;  // Standard 11-bit ID
  }
  tx.can_dlc = dlc;
  for (uint8_t i = 0; i < dlc; i++) {
    tx.data[i] = data[i];
  }

  // Retry up to 50 times if TX buffers are busy
  for (int attempt = 0; attempt < 50; ++attempt) {
    MCP2515::ERROR r = mcp2515.sendMessage(&tx);
    
    if (r == MCP2515::ERROR_OK) {
      return true;
    }
    
    if (r == MCP2515::ERROR_ALLTXBUSY) {
      delay(5);
      continue;
    }
    
    Serial.print("✗ Send error: ");
    Serial.println(r);
    return false;
  }
  
  Serial.println("✗ TX timeout");
  return false;
}

// Check for STSPIN response
static void checkForResponse(unsigned long timeoutMs = 500) {
  struct can_frame rx;
  unsigned long start = millis();
  
  while (millis() - start < timeoutMs) {
    if (mcp2515.readMessage(&rx) == MCP2515::ERROR_OK) {
      bool isExtended = (rx.can_id & CAN_EFF_FLAG) != 0;
      uint32_t id = rx.can_id & (isExtended ? CAN_EFF_MASK : CAN_SFF_MASK);
      
      Serial.print("✓ RX: ID=");
      if (isExtended) {
        Serial.print(id);
        Serial.print(" (Ext)");
      } else {
        Serial.print("0x");
        Serial.print(id, HEX);
      }
      Serial.print(" DLC=");
      Serial.print(rx.can_dlc);
      Serial.print(" Data: ");
      for (int i = 0; i < rx.can_dlc; i++) {
        if (rx.data[i] < 0x10) Serial.print("0");
        Serial.print(rx.data[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
      
      if (id == STSPIN_RESPONSE_ID && isExtended) {
        Serial.println("  ^ STSPIN Response!");
      }
      return;
    }
    delay(5);
  }
  Serial.println("No response (timeout)");
}

static String readLineWithEcho() {
  while (Serial.available()) {
    Serial.read();
    delay(1);
  }
  
  String line = "";
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      
      if (c == '\b' || c == 127) {
        if (line.length() > 0) {
          line.remove(line.length() - 1);
          Serial.print("\b \b");
        }
        continue;
      }
      
      if (c == '\n' || c == '\r') {
        if (line.length() > 0) {
          Serial.println();
          return line;
        }
        continue;
      }
      
      if (isPrintable(c)) {
        Serial.print(c);
        line += c;
      }
    }
    delay(1);
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  delay(500);
  Serial.println("\n=== ESP32 CAN Sender for STSPIN ===");
  Serial.println("Commands:");
  Serial.println("  1 - Send test message to STSPIN");
  Serial.println("  2 - Send custom hex data");
  Serial.println("  3 - Motor START");
  Serial.println("  4 - Motor STOP");
  Serial.println("  5 - Listen mode");
  Serial.println("  6 - Loopback test (no CAN bus needed)");
  Serial.println("  7 - Listen-only mode (passive, no ACK)");
  Serial.println();

  SPI.begin();
  
  Serial.println("Initializing MCP2515...");
  mcp2515.reset();
  delay(10);
  
  // Use 16MHz crystal @ 500kbps to match STSPIN
  MCP2515::ERROR result = mcp2515.setBitrate(CAN_500KBPS, MCP_16MHZ);
  if (result == MCP2515::ERROR_OK) {
    Serial.println("✓ Bitrate: 500kbps @ 16MHz (matches STSPIN)");
  } else {
    Serial.println("✗ Bitrate config failed!");
  }

  result = mcp2515.setNormalMode();
  if (result == MCP2515::ERROR_OK) {
    Serial.println("✓ MCP2515 Normal mode");
  } else {
    Serial.println("✗ Mode set failed!");
  }
  
  Serial.println("\nReady. Enter command (1-5):\n");
}

void loop() {
  Serial.print("> ");
  String cmd = readLineWithEcho();
  
  if (cmd.length() == 0) return;
  
  int choice = cmd.toInt();
  
  switch (choice) {
    case 1: {
      // Send test message - STSPIN will respond with ID 10101
      uint8_t testData[] = {0x01, 0x02, 0x03, 0x04};
      Serial.println("Sending test to STSPIN (ID 0x100)...");
      if (sendFrame(0x100, testData, 4, false)) {
        Serial.println("✓ Sent");
        checkForResponse();
      }
      break;
    }
    
    case 2: {
      // Custom message
      Serial.print("CAN ID (hex): ");
      String idStr = readLineWithEcho();
      uint32_t canId = strtoul(idStr.c_str(), NULL, 16);
      
      Serial.print("Extended ID? (y/n): ");
      String extStr = readLineWithEcho();
      bool extended = (extStr.charAt(0) == 'y' || extStr.charAt(0) == 'Y');
      
      Serial.print("Data (hex, space-sep): ");
      String dataStr = readLineWithEcho();
      
      uint8_t data[8];
      uint8_t dlc = 0;
      char* token = strtok((char*)dataStr.c_str(), " ");
      while (token && dlc < 8) {
        data[dlc++] = strtoul(token, NULL, 16);
        token = strtok(NULL, " ");
      }
      
      if (dlc > 0) {
        Serial.print("Sending ID=");
        Serial.print(canId, HEX);
        Serial.print(extended ? " (Ext)" : " (Std)");
        Serial.print(" DLC=");
        Serial.println(dlc);
        
        if (sendFrame(canId, data, dlc, extended)) {
          Serial.println("✓ Sent");
          checkForResponse();
        }
      }
      break;
    }
    
    case 3: {
      // Motor start
      uint8_t startCmd[] = {0x01};
      Serial.println("Sending Motor START...");
      if (sendFrame(0x100, startCmd, 1, false)) {
        Serial.println("✓ Sent");
        checkForResponse();
      }
      break;
    }
    
    case 4: {
      // Motor stop
      uint8_t stopCmd[] = {0x00};
      Serial.println("Sending Motor STOP...");
      if (sendFrame(0x100, stopCmd, 1, false)) {
        Serial.println("✓ Sent");
        checkForResponse();
      }
      break;
    }
    
    case 5: {
      // Listen mode
      Serial.println("Listening... (press any key to stop)");
      while (!Serial.available()) {
        struct can_frame rx;
        if (mcp2515.readMessage(&rx) == MCP2515::ERROR_OK) {
          bool isExt = (rx.can_id & CAN_EFF_FLAG) != 0;
          uint32_t id = rx.can_id & (isExt ? CAN_EFF_MASK : CAN_SFF_MASK);
          
          Serial.print("[RX] ID=");
          if (isExt) {
            Serial.print(id);
            Serial.print("(Ext)");
          } else {
            Serial.print("0x");
            Serial.print(id, HEX);
          }
          Serial.print(" Data: ");
          for (int i = 0; i < rx.can_dlc; i++) {
            if (rx.data[i] < 0x10) Serial.print("0");
            Serial.print(rx.data[i], HEX);
            Serial.print(" ");
          }
          Serial.println();
        }
        delay(10);
      }
      while (Serial.available()) Serial.read();
      Serial.println("Stopped.\n");
      break;
    }
    
    case 6: {
      // Loopback test - tests MCP2515 without needing CAN bus
      Serial.println("Running LOOPBACK test...");
      Serial.println("(This tests the MCP2515 chip without CAN bus)");
      
      // Switch to loopback mode
      mcp2515.setLoopbackMode();
      delay(10);
      
      // Send a test message
      struct can_frame tx;
      tx.can_id = 0x123;
      tx.can_dlc = 4;
      tx.data[0] = 0xAA;
      tx.data[1] = 0xBB;
      tx.data[2] = 0xCC;
      tx.data[3] = 0xDD;
      
      MCP2515::ERROR sendResult = mcp2515.sendMessage(&tx);
      if (sendResult == MCP2515::ERROR_OK) {
        Serial.println("✓ TX OK in loopback");
        
        // Try to receive it back
        delay(10);
        struct can_frame rx;
        if (mcp2515.readMessage(&rx) == MCP2515::ERROR_OK) {
          Serial.print("✓ RX OK! ID=0x");
          Serial.print(rx.can_id, HEX);
          Serial.print(" Data: ");
          for (int i = 0; i < rx.can_dlc; i++) {
            Serial.print(rx.data[i], HEX);
            Serial.print(" ");
          }
          Serial.println();
          Serial.println("*** MCP2515 HARDWARE IS WORKING ***");
        } else {
          Serial.println("✗ RX failed in loopback");
        }
      } else {
        Serial.print("✗ TX failed in loopback: ");
        Serial.println(sendResult);
        Serial.println("*** MCP2515 HARDWARE PROBLEM ***");
      }
      
      // Switch back to normal mode
      mcp2515.setNormalMode();
      Serial.println("Back to Normal mode.\n");
      break;
    }
    
    case 7: {
      // Listen-only mode - passive monitoring, won't ACK frames
      Serial.println("LISTEN-ONLY mode (passive, won't ACK)");
      Serial.println("This can see traffic even without proper termination.");
      Serial.println("Press any key to stop...\n");
      
      mcp2515.setListenOnlyMode();
      delay(10);
      
      while (!Serial.available()) {
        struct can_frame rx;
        if (mcp2515.readMessage(&rx) == MCP2515::ERROR_OK) {
          bool isExt = (rx.can_id & CAN_EFF_FLAG) != 0;
          uint32_t id = rx.can_id & (isExt ? CAN_EFF_MASK : CAN_SFF_MASK);
          
          Serial.print("[RX] ID=");
          if (isExt) {
            Serial.print(id);
            Serial.print("(Ext)");
          } else {
            Serial.print("0x");
            Serial.print(id, HEX);
          }
          Serial.print(" DLC=");
          Serial.print(rx.can_dlc);
          Serial.print(" Data: ");
          for (int i = 0; i < rx.can_dlc; i++) {
            if (rx.data[i] < 0x10) Serial.print("0");
            Serial.print(rx.data[i], HEX);
            Serial.print(" ");
          }
          Serial.println();
        }
        delay(10);
      }
      while (Serial.available()) Serial.read();
      
      mcp2515.setNormalMode();
      Serial.println("Back to Normal mode.\n");
      break;
    }
    
    default:
      Serial.println("Invalid. Enter 1-5.");
      break;
  }
  
  Serial.println();
}

#endif // ROLE_SENDER
