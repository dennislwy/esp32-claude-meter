#include "factory_reset.h"

#include <nvs_flash.h>
#include "history.h"
#include "secret_store.h"

void factoryReset()
{
  // First, so the session key in RTC RAM is zeroed too: erasing NVS cannot
  // reach it, and it would otherwise survive the restart
  secret_store::wipeCredentials();
  historyErase();
  // The whole partition, not just this firmware's namespaces, so the Wi-Fi
  // driver's own copy and anything an earlier firmware left go with it
  nvs_flash_erase();
  // Usable again for whatever runs before the caller restarts
  nvs_flash_init();
}
