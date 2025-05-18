# ESP32 BLE Broadcast Authentication

This repository presents an authentication mechanism for BLE (Bluetooth Low Energy) broadcast non-connective transmission.

## Overview

The proposed mechanism utilizes **AES-CTR encryption** to secure transmitted data. The **AES symmetric key** (128-bit) is divided into **four equal 4-byte fragments**, which are **scrambled** and sent along with the data.

To decrypt the broadcasted PDUs (Protocol Data Units), the **Observer** must collect and unscramble all key fragments. Additionally, the **Broadcaster periodically changes the encryption key**, ensuring dynamic security. Each key is linked to a **session ID**, which is also transmitted in the PDUs.

## Authentication Mechanism

Beyond encryption, an additional authentication step is implemented using **advertising intervals derived from the session ID**:

1. The **Broadcaster** generates a random **session ID** and computes the corresponding **advertising interval**.
2. The **Observer** evaluates the **arrival times** of received packets to estimate the advertising interval.
3. The **Observer** extracts the session ID from the PDU and checks whether the **evaluated interval falls within an acceptable time window**.

This approach enhances security by requiring both **cryptographic key reconstruction** and **advertising interval validation** for successful decryption and authentication.
