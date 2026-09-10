#pragma once

#include <stdbool.h>

typedef enum {
    WIFI_PROVISION_IDLE = 0,
    WIFI_PROVISION_STARTING,
    WIFI_PROVISION_WAITING,
    WIFI_PROVISION_CONNECTING,
    WIFI_PROVISION_CONNECTED,
    WIFI_PROVISION_FAILED,
} wifi_provision_state_t;

void wifi_provision_prepare(void);
void wifi_provision_auto_start(void);
void wifi_provision_start(void);
void wifi_provision_retry(void);
void wifi_provision_stop(void);
bool wifi_provision_is_connected(void);
wifi_provision_state_t wifi_provision_state(void);
const char *wifi_provision_service_name(void);
const char *wifi_provision_state_text(void);
