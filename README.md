# CAN Firmware Testing: Multi-Receiver Messaging

This project now supports one sender and up to five receivers over MCP2515 CAN (500 kbps, 16 MHz crystal), with variable-length messages segmented across CAN frames.

## Environments

PlatformIO environments were added:

- `sender` – interactive sender (choose target 1..5, type any-length message)
- `receiver1` .. `receiver5` – receiver firmware with `RECEIVER_ID` set accordingly

Existing `pico32` env is left intact for backward compatibility.

## Wiring (ESP32 Pico Kit v4.1 + MCP2515)

- CS  -> GPIO 5
- MOSI -> GPIO 23
- MISO -> GPIO 19
- SCK -> GPIO 18

## Build & Upload

- Sender:
  - Select `env:sender` and upload to the sender ESP32.
- Receivers (repeat per device):
  - Select one of `env:receiver1`..`env:receiver5` matching the unit you’re flashing and upload.

## Using the Sender

Open the Serial Monitor at 115200 baud. For each message:

1. When prompted, enter target ID (1–5) and press Enter.
2. Type your message and press Enter. Any length is supported.

The sender splits the message across frames and sends to CAN ID `0x200 + targetId`.

## Protocol

Standard 11-bit CAN IDs:
- Targeted ID: `0x200 + receiverId` (1..5)

Frames:
- Start frame (DLC 4–8):
  - `data[0] = 0xAA`
  - `data[1] = totalLen low byte`
  - `data[2] = totalLen high byte`
  - `data[3] = seq (=0)`
  - `data[4..] = first payload bytes (up to 4)`
- Continuation frame (DLC 2–8):
  - `data[0] = 0xCC`
  - `data[1] = seq (1,2,...)`
  - `data[2..] = payload (up to 6)`

Receivers reassemble until `totalLen` bytes are collected, then print the full message.

## Notes

- Max message length capped to 65535 bytes by protocol, and a 2KB receive buffer by default (`receiver.cpp: MAX_MESSAGE`). Increase carefully based on available RAM.
- Simple sequence checking resets on mismatch. This keeps logic robust against lost frames.
- You can add MCP2515 acceptance filters later to only accept your specific ID; current code filters in software by CAN ID.
