idf_component_register(SRCS "mqtt_task.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_eth driver esp_netif json main)
