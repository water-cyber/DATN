#ifndef ETHERNET_W5500_H
#define ETHERNET_W5500_H
#include "esp_netif.h"
#include "esp_err.h"

void ethernet_w5500_init(void);
extern esp_netif_t *eth_netif;
#endif // ETHERNET_W5500_H
