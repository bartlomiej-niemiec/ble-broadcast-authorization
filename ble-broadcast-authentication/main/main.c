#include "ble_sender.h"
#include "ble_receiver.h"
#include "ble_common.h"

#define RECEIVER

void app_main(void)
{
    ble_init();
    #ifdef SENDER
        ble_start_sender();
    #else
        ble_start_receiver();
    #endif
}
