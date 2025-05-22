# ESP32 BLE Broadcast Authentication

This repository presents an authentication mechanism for BLE (Bluetooth Low Energy) broadcast non-connective transmission.

## Overview

The proposed mechanism utilizes **AES-CTR encryption** to secure transmitted data. The **AES symmetric key** (128-bit) is divided into **four equal 4-byte fragments**, which are **scrambled** and sent along with the data.

To decrypt the broadcasted PDUs (Protocol Data Units), the **Observer** must collect and unscramble all key fragments. Additionally, the **Broadcaster periodically changes the encryption key**, ensuring dynamic security. Each key is linked to a **session ID**, which is also transmitted in the PDUs.

## Rolling Key Mechanism

Each encryption key is used for a limited time or number of packets and then replaced by a new one. This mechanism enhances security by limiting the lifespan of each key.

* A new **session ID** is generated whenever the key is rotated.
* The **advertising interval** is derived from the session ID: for example, interval = session ID × 50 ms.
* The **Broadcaster** updates the session ID, key, and interval simultaneously.
* The **Observer** detects new sessions by observing changes in packet timing and session ID.

This rolling mechanism ensures that even if a key is compromised, its utility is short-lived.

## Authentication Mechanism

Beyond encryption, an additional authentication step is implemented using **advertising intervals derived from the session ID**:

1. The **Broadcaster** generates a random **session ID** and computes the corresponding **advertising interval**.
2. The **Observer** evaluates the **arrival times** of received packets to estimate the advertising interval.
3. The **Observer** extracts the session ID from the PDU and checks whether the **evaluated interval falls within an acceptable time window**.

This approach enhances security by requiring both **cryptographic key reconstruction** and **advertising interval validation** for successful decryption and authentication.

## Packet Structure and HMAC

* Key fragments are protected using **XOR masking** and a **shortened HMAC (4 bytes)**.
* Only packets containing key fragments include an HMAC for verification.
* The HMAC is computed from the original (unmasked) fragment and a shared secret.
* Observers must reconstruct the full key and verify the HMACs before decrypting data packets.

## Authentication Session Flow

Below is a session flow diagram that illustrates the unidirectional communication from Broadcaster to Observer. Since BLE broadcast communication is non-connective, the Observer only listens and never responds.

Steps:

1. **Broadcaster (ESP32)** transmits encrypted data packets along with scrambled key fragments.
2. **Key fragments** are protected with XOR masking and verified using HMAC (4 bytes).
3. **Observer (ESP32)** listens for packets, reconstructs the key after collecting all fragments, verifies HMACs, and decrypts the payload.
4. The **Observer also verifies packet arrival intervals**, linked to the session ID, for additional authentication.

## Authentication on component diagram

On the component diagram below presented how the authentication process looks like.

![alt text](https://github.com/bartlomiej-niemiec/ble-broadcast-authorization/blob/master/docs/ReceiverComponentDiagram.png "Authentication Component Diagram")

## Project Structure

This project is built on ESP32 devices using the ESP-IDF framework.

"projects" directory contains three sub-projects:

* `receiver_app` – ESP32 configured as a BLE Observer, listening to encrypted broadcast communication.
* `sender_app` – ESP32 configured as a BLE Broadcaster, initiating encrypted communication and rotating session keys every N packets.
* `sender_app_2nd` – Same as `sender_app`, used in a two Broadcaster / one Observer scenario.

Projects are designed to work in conjunction with Python scripts for serial initialization:

* Connect devices via USB.
* Use `test_app_receiver.py` and `test_app_sender.py` to start communication.
* Devices log sent/received packet counts via serial.

## Building

To build a project, ensure the ESP-IDF environment is set up.

Edit `projects/CMakeLists.txt` to choose a project to build:

```cmake
idf_component_register(
        SRCS "./receiver_app/receiver_main.c"
        INCLUDE_DIRS "./receiver_app"
        REQUIRES "ble_broadcast_security_processing_engine" "core" "utils" "ble_broadcast_controller" "test_framework"
    )

# idf_component_register(
#     SRCS "./sender_app/sender_main.c"
#     INCLUDE_DIRS "./sender_app"
#     REQUIRES "ble_security_payload_encryption" "core" "ble_broadcast_controller" "pc_communication_serial" "test_framework"
#     REQUIRES esp_timer
# )

# idf_component_register(
#     SRCS "./sender_app_2nd/sender_main.c"
#     INCLUDE_DIRS "./sender_app_2nd"
#     REQUIRES "ble_security_payload_encryption" "core" "ble_broadcast_controller" "pc_communication_serial" "test_framework"
#     REQUIRES esp_timer
# )
```

---
