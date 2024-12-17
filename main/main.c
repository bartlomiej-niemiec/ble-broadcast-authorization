#include "ble_common.h"

#define SENDER

#ifdef RECEIVER
    #include "ble_receiver.h"
#else
    #include "ble_sender.h"
#endif

void app_main(void)
{
    ble_init();
    #ifdef RECEIVER
        ble_start_receiver();
    #else
        ble_start_sender();
    #endif
}
