#ifndef BEACON_MARKER_H
#define BEACON_MARKER_H

#include <stdint.h>

#define MARKER_SIZE 4

typedef struct {
    uint8_t marker[MARKER_SIZE];             // unique ID of PDU
}__attribute__((packed)) beacon_marker;

#define MARKER_STRUCT_SIZE sizeof(beacon_marker)

extern beacon_marker my_marker;

#endif