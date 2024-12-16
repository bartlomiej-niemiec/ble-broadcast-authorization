#include "ble_common.h"

#define RECEIVER

#ifdef RECEIVER
    #include "ble_receiver.h"
#else
    #include "ble_sender.h"
#endif

void app_main(void)
{
    ble_init();
    #ifdef SENDER
        ble_start_sender();
    #else
        ble_start_receiver();
    #endif
}
