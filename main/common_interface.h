#ifndef COMMON_INTERFACE_H
#define COMMON_INTERFACE_H


typedef struct
{
    bool status_eth;
    bool status_wifi;
    bool pub_led;
    bool relay;
    bool mqtt_connect;
    bool status_ap;

} gateway_data_t;

extern int led_pub_state[5];
extern gateway_data_t gateway_data;
#endif // COMMON_INTERFACE_H