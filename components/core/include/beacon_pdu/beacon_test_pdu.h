#ifndef BEACON_TEST_PDU_DATA_H
#define BEACON_TEST_PDU_DATA_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define TEST_MARKER_SIZE 10

typedef struct {
    uint8_t marker[TEST_MARKER_SIZE]; 
} pdu_test_marker;

typedef struct {
    uint32_t command;
} test_command;

typedef enum {
    START_TEST,
    END_TEST
} test_commands;

typedef struct {
    pdu_test_marker marker;
    test_command cmd;
}__attribute__((packed)) beacon_test_pdu;


esp_err_t build_test_start_pdu (beacon_test_pdu *test_pdu);

esp_err_t build_test_end_pdu (beacon_test_pdu *test_pdu);

esp_err_t is_test_pdu(uint8_t *data, size_t data_len);

#endif