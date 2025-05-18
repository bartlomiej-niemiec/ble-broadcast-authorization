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

## Authentication on component diagram

On the component diagram below presented how the authentication process looks like.

![alt text](https://github.com/bartlomiej-niemiec/ble-broadcast-authorization/blob/master/docs/ReceiverComponentDiagram.png "Authentication Component Diagram")

## Project structure

Project was build on ESP32 device with ESP-IDF framework.

"projects" directory contains 3 projects:
* receiver_app - ESP32 device is configured as BLE Observer and listenes to encrypted broadcast communication
* sender_app - ESP32 device is configured as BLE Broacaster and initiates encrypted broadcast communication. ESP32 will 
               send encrypted PDU and rotates session key every N send PDUs.
* sencer_app_2nd - ESP32 will behaviour the same as in 'sender_app' project however it's used in 2 Broadcasters and 1 Observer scenario

Projects are created to cooperate with python script which initiates devices (1 Broadcaster ("sender_app") and 1 Observer ("receiver_app"))
to start operate. Both devices need to be connected with USB cable, then running 'test_app_receiver.py' and 'test_app_sender.py' with configured parameters would
send information via serial to start communication (Broadcaster -> Observer).
Each device will log data via serial how many has been sent and received.

## Building

To build one of the project it's required to have ESP-IDF environment.

If the environment is set then choice of the project to build should be done by uncomment one of the project in CMake file and comment the rest.

CMake filepath: projects/CMakeLists.txt 

Example:
```
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
