#ifdef ROLE_SENDER
/*
 * CAN Bus Sender with Multi-Receiver Targeting and Segmentation
 * - Choose target receiver (1..5) at runtime via Serial
 * - Send messages of any length by splitting across multiple CAN frames
 *
 * Protocol (standard 11-bit CAN IDs):
 * - CAN ID: 0x200 + targetId (1..5)
 * - Start frame: [0]=0xAA, [1]=lenLow, [2]=lenHigh, [3]=seq(0), [4..]=payload (up to 4 bytes)
 * - Cont frame:  [0]=0xCC, [1]=seq(1..), [2..]=payload (up to 6 bytes)
 * - Complete when receiver collects totalLen bytes
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
static const uint16_t CAN_BASE_ID = 0x200; // IDs 0x201..0x205

static const uint8_t FRAME_MAGIC_START = 0xAA;
static const uint8_t FRAME_MAGIC_CONT  = 0xCC;

MCP2515 mcp2515(CAN_CS_PIN);

static bool sendFrame(const struct can_frame &frm) {
  // Retry logic to handle TX busy
  const int maxRetries = 50;
  for (int attempt = 0; attempt < maxRetries; ++attempt) {
    MCP2515::ERROR r = mcp2515.sendMessage(const_cast<struct can_frame*>(&frm));
    
    if (r == MCP2515::ERROR_OK) {
      return true;
    }
    
    if (r == MCP2515::ERROR_ALLTXBUSY) {
      // Wait a bit and retry
      delay(5);
      continue;
    }
    
    // Other errors are fatal
    Serial.print("✗ Send failed: ");
    switch(r) {
      case MCP2515::ERROR_FAILINIT: Serial.println("Initialization failed"); break;
      case MCP2515::ERROR_FAILTX: Serial.println("Transmission failed"); break;
      default: Serial.println("Unknown error"); break;
    }
    return false;
  }
  
  Serial.println("✗ Send failed: TX buffers busy (timeout)");
  return false;
}

static bool sendMessageTo(uint8_t targetId, const uint8_t* data, uint16_t len) {
  if (targetId < 1 || targetId > 5) {
    Serial.println("Target ID must be 1..5");
    return false;
  }

  if (len > 65535) {
    Serial.println("Message too long (max 65535 bytes)");
    return false;
  }

  const uint16_t canId = CAN_BASE_ID + targetId;
  struct can_frame tx;
  tx.can_id = canId;

  uint8_t seq = 0;
  uint16_t offset = 0;

  // Start frame
  const uint8_t firstChunk = (len >= 4) ? 4 : (uint8_t)len;
  tx.data[0] = FRAME_MAGIC_START;
  tx.data[1] = (uint8_t)(len & 0xFF);
  tx.data[2] = (uint8_t)((len >> 8) & 0xFF);
  tx.data[3] = seq; // 0
  for (uint8_t i = 0; i < firstChunk; ++i) {
    tx.data[4 + i] = data[i];
  }
  tx.can_dlc = 4 + firstChunk; // 4..8
  if (!sendFrame(tx)) return false;
  offset += firstChunk;
  delay(10); // Give receiver time to process start frame

  // Continuation frames
  while (offset < len) {
    seq++;
    const uint8_t chunk = (len - offset >= 6) ? 6 : (uint8_t)(len - offset);
    tx.data[0] = FRAME_MAGIC_CONT;
    tx.data[1] = seq;
    for (uint8_t i = 0; i < chunk; ++i) {
      tx.data[2 + i] = data[offset + i];
    }
    tx.can_dlc = 2 + chunk; // 2..8
    if (!sendFrame(tx)) return false;
    offset += chunk;
    delay(10); // Increased pacing to prevent TX buffer saturation
  }

  return true;
}

static String readLineWithEcho() {
  // Flush any leftover characters in the serial buffer
  while (Serial.available()) {
    Serial.read();
    delay(1);
  }
  
  // Read character by character and echo back to user
  String line = "";
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      
      // Handle backspace
      if (c == '\b' || c == 127) {
        if (line.length() > 0) {
          line.remove(line.length() - 1);
          Serial.print("\b \b"); // Erase character on screen
        }
        continue;
      }
      
      // Handle newline/carriage return - but only if we have some content
      // or if the line is already being built
      if (c == '\n' || c == '\r') {
        if (line.length() > 0 || c == '\r') {
          Serial.println(); // Move to next line
          return line;
        }
        // Ignore leading newlines
        continue;
      }
      
      // Regular character - echo and append
      if (isPrintable(c)) {
        Serial.print(c);
        line += c;
      }
    }
    delay(1);
  }
}

static int readTargetIdBlocking() {
  while (true) {
    Serial.print("Enter target ID (1-5): ");
    String s = readLineWithEcho();
    if (s.length() == 0) continue;
    int id = s.toInt();
    if (id >= 1 && id <= 5) return id;
    Serial.println("Invalid ID. Please enter a number 1..5.");
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  delay(600);
  Serial.println("\n=== CAN Bus Sender ===");
  Serial.println("- Choose a receiver 1..5");
  Serial.println("- Type any length message to send\n");

  SPI.begin();
  
  Serial.println("Resetting MCP2515...");
  mcp2515.reset();
  delay(100);
  
  // Try 16MHz first, then 8MHz if that fails
  MCP2515::ERROR result = mcp2515.setBitrate(CAN_500KBPS, MCP_16MHZ);
  if (result == MCP2515::ERROR_OK) {
    Serial.println("✓ Bitrate set to 500kbps @ 16MHz");
  } else {
    Serial.println("✗ 16MHz failed, trying 8MHz...");
    result = mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
    if (result == MCP2515::ERROR_OK) {
      Serial.println("✓ Bitrate set to 500kbps @ 8MHz");
    } else {
      Serial.println("✗ Error setting bitrate - check SPI wiring!");
    }
  }

  result = mcp2515.setNormalMode();
  if (result == MCP2515::ERROR_OK) {
    Serial.println("✓ MCP2515 in Normal mode");
  } else {
    Serial.println("✗ Error setting Normal mode - check wiring!");
  }
  
  // Optionally test in loopback mode first (for hardware verification)
  // Uncomment the next 3 lines to test without needing a receiver connected:
  // mcp2515.setLoopbackMode();
  // Serial.println("⚠ Running in LOOPBACK mode (testing only - no CAN bus needed)");
  
  // Check if we can read back a register to verify SPI communication
  Serial.println("\nDiagnostics:");
  Serial.println("- Verify 120Ω termination resistors at BOTH ends of CAN bus");
  Serial.println("- Verify at least one receiver is connected and powered");
  Serial.println("- Check SPI wiring: CS=GPIO5, MOSI=23, MISO=19, SCK=18");
  Serial.println();
}

void loop() {
  const int target = readTargetIdBlocking();
  Serial.print("Enter message text: ");
  String msg = readLineWithEcho();

  // Convert to bytes
  const uint16_t len = (uint16_t)msg.length();
  
  if (len == 0) {
    Serial.println("Empty message. Skipped.\n");
    return;
  }
  
  Serial.print("Sending "); Serial.print(len); Serial.print(" bytes to receiver "); Serial.print(target);
  Serial.print(": \""); Serial.print(msg); Serial.println("\"");

  if (sendMessageTo((uint8_t)target, (const uint8_t*)msg.c_str(), len)) {
    Serial.println("✓ Message sent successfully\n");
  } else {
    Serial.println("✗ Failed to send message\n");
  }

  // Allow next command
  delay(100);
}

#endif // ROLE_SENDER
