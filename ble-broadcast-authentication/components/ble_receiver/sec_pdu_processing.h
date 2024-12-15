#ifndef SEC_PDU_PROCESSING_H
#define SEC_PDU_PROCESSING_H

#include "beacon_pdu_data.h"
#include <stdint.h>
#include <stddef.h>

int start_up_sec_processing();

int process_sec_pdu(beacon_pdu_data* pdu);

#endif