#include "ble_receiver.h"
#include "ble_common.h"

void app_main(void)
{
    ble_init();
    ble_start_receiver();
}
