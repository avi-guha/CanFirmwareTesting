#ifdef ROLE_RECEIVER
/*
 * CAN Bus Receiver supporting multi-frame segmented messages
 * - Compile with -D RECEIVER_ID=1..5
 * - Listens on CAN ID 0x200 + RECEIVER_ID
 *
 * Protocol (must match sender):
 * Start frame (magic 0xAA): [0]=0xAA, [1]=lenLow, [2]=lenHigh, [3]=seq(0), [4..]=payload
 * Continuation (magic 0xCC): [0]=0xCC, [1]=seq(>=1), [2..]=payload
 *
 * Assembles message in a buffer up to MAX_MESSAGE (configurable)
 */

#include <Arduino.h>
#include <SPI.h>
#include <mcp2515.h>

#ifndef RECEIVER_ID
#error "RECEIVER_ID must be defined (1..5)"

#endif

#define CAN_CS_PIN 5
static const uint16_t CAN_BASE_ID = 0x200; // base for targeted messages
static const uint8_t FRAME_MAGIC_START = 0xAA;
static const uint8_t FRAME_MAGIC_CONT  = 0xCC;

// Adjust as needed. Large buffers consume RAM; ESP32 usually fine.
static const uint16_t MAX_MESSAGE = 2048; // 2KB cap

MCP2515 mcp2515(CAN_CS_PIN);

static uint8_t  buffer[MAX_MESSAGE];
static uint16_t expectedLen = 0;
static uint16_t receivedLen = 0;
static uint8_t  nextSeq = 0;
static bool     assembling = false;

static void resetAssembly() {
  expectedLen = 0;
  receivedLen = 0;
  nextSeq = 0;
  assembling = false;
}

static void handleStartFrame(const struct can_frame &frm) {
  if (frm.can_dlc < 4) {
    Serial.println("Start frame too short");
    return;
  }
  expectedLen = (uint16_t)frm.data[1] | ((uint16_t)frm.data[2] << 8);
  nextSeq = 1; // next expected continuation seq
  receivedLen = 0;
  assembling = true;

  if (expectedLen > MAX_MESSAGE) {
    Serial.print("Incoming message length "); Serial.print(expectedLen); Serial.println(" exceeds buffer. Dropping.");
    resetAssembly();
    return;
  }

  uint8_t payload = frm.can_dlc - 4; // bytes after header
  for (uint8_t i = 0; i < payload; ++i) {
    buffer[receivedLen++] = frm.data[4 + i];
  }

  Serial.print("Start message len="); Serial.print(expectedLen);
  Serial.print(" firstChunk="); Serial.print(payload); Serial.println();

  if (receivedLen >= expectedLen) {
    // Complete in one frame
    buffer[receivedLen] = '\0';
    Serial.println("\n┌─────────────────────────────────");
    Serial.print("│ Receiver #"); Serial.print(RECEIVER_ID); Serial.println(" - Message Received:");
    Serial.print("│ Length: "); Serial.print(expectedLen); Serial.println(" bytes");
    Serial.println("├─────────────────────────────────");
    Serial.print("│ "); Serial.println((char*)buffer);
    Serial.println("└─────────────────────────────────\n");
    resetAssembly();
  }
}

static void handleContFrame(const struct can_frame &frm) {
  if (!assembling) {
    Serial.println("Unexpected continuation (no assembly in progress)");
    return;
  }
  if (frm.can_dlc < 2) {
    Serial.println("Continuation frame too short");
    resetAssembly();
    return;
  }
  uint8_t seq = frm.data[1];
  if (seq != nextSeq) {
    Serial.print("Sequence mismatch. Expected "); Serial.print(nextSeq); Serial.print(" got "); Serial.println(seq);
    resetAssembly();
    return;
  }
  nextSeq++;
  uint8_t payload = frm.can_dlc - 2;
  for (uint8_t i = 0; i < payload && receivedLen < expectedLen; ++i) {
    buffer[receivedLen++] = frm.data[2 + i];
  }
  Serial.print("Added chunk seq="); Serial.print(seq); Serial.print(" size="); Serial.print(payload); Serial.print(" progress="); Serial.print(receivedLen); Serial.print("/"); Serial.println(expectedLen);

  if (receivedLen >= expectedLen) {
    buffer[receivedLen] = '\0';
    Serial.println("\n┌─────────────────────────────────");
    Serial.print("│ Receiver #"); Serial.print(RECEIVER_ID); Serial.println(" - Message Received:");
    Serial.print("│ Length: "); Serial.print(expectedLen); Serial.println(" bytes");
    Serial.println("├─────────────────────────────────");
    Serial.print("│ "); Serial.println((char*)buffer);
    Serial.println("└─────────────────────────────────\n");
    resetAssembly();
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }
  delay(600);
  Serial.println();
  Serial.print("=== CAN Receiver #"); Serial.print(RECEIVER_ID); Serial.println(" ===");
  Serial.print("Listening on CAN ID 0x"); Serial.println((CAN_BASE_ID + RECEIVER_ID), HEX);

  SPI.begin();
  mcp2515.reset();
  MCP2515::ERROR result = mcp2515.setBitrate(CAN_500KBPS, MCP_16MHZ);
  if (result == MCP2515::ERROR_OK) Serial.println("✓ Bitrate set to 500kbps @ 16MHz"); else Serial.println("✗ Error setting bitrate");
  result = mcp2515.setNormalMode();
  if (result == MCP2515::ERROR_OK) Serial.println("✓ MCP2515 initialized successfully!"); else Serial.println("✗ Error: Check wiring!");
  Serial.println("Ready. Waiting for messages...\n");
}

void loop() {
  struct can_frame rx;
  if (mcp2515.readMessage(&rx) == MCP2515::ERROR_OK) {
    // Filter by our target ID
    if (rx.can_id != (CAN_BASE_ID + RECEIVER_ID)) {
      // Not for us; could log lightly
      return;
    }

    uint8_t magic = rx.data[0];
    if (magic == FRAME_MAGIC_START) {
      handleStartFrame(rx);
    } else if (magic == FRAME_MAGIC_CONT) {
      handleContFrame(rx);
    } else {
      Serial.print("Unknown frame magic 0x"); Serial.println(magic, HEX);
    }
  }
  delay(5);
}

#endif // ROLE_RECEIVER
