#include "ethernet_w5500.h"
#include "esp_log.h"
#include "esp_eth.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "common_interface.h"
static const char *TAG = "ETH_W5500";

#define PIN_MISO    19
#define PIN_MOSI    23
#define PIN_SCLK    18
#define PIN_CS      5
#define PIN_INT     -1
#define PIN_RST     -1

static esp_eth_handle_t eth_handle = NULL;
esp_netif_t *eth_netif = NULL;
spi_device_interface_config_t devcfg;

static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data) {
    uint8_t mac_addr[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
        case ETHERNET_EVENT_CONNECTED:
            gateway_data.status_eth = true;
            esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
            ESP_LOGI(TAG, "Ethernet connected, MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                     mac_addr[0], mac_addr[1], mac_addr[2], 
                     mac_addr[3], mac_addr[4], mac_addr[5]);
            break;

        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Ethernet disconnected");
            gateway_data.status_eth = false;
            break;

        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet started");
            break;

        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet stopped");
            gateway_data.status_eth = false;
            break;
    }
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Got IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
}

spi_device_handle_t configure_spi(void)
{
    // Cấu hình bus SPI
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_MISO,
        .mosi_io_num = PIN_MOSI,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,  // Không dùng D2
        .quadhd_io_num = -1,  // Không dùng D3
        .max_transfer_sz = 4096 // Kích thước tối đa
    };
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    // Cấu hình thiết bị SPI
    devcfg = (spi_device_interface_config_t){
        .command_bits = 16, // Địa chỉ trong W5500 SPI frame
        .address_bits = 8,  // Giai đoạn điều khiển trong W5500 SPI frame
        .mode = 0,
        .clock_speed_hz = 8 * 1000 * 1000,	
        .spics_io_num = PIN_CS,
        .queue_size = 20
    };

    // Cài đặt ISR cho GPIO nếu cần
    //gpio_install_isr_service(0);

    spi_device_handle_t spi_handle = NULL;
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle));

    return spi_handle;
}
esp_eth_handle_t configure_ethernet(spi_device_handle_t spi_handle)
{
    // Cấu hình MAC
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.sw_reset_timeout_ms = 10;

    // Cấu hình PHY
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.autonego_timeout_ms = 0;
    phy_config.reset_gpio_num = -1;

    // Cấu hình W5500
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(SPI2_HOST, &devcfg);
    w5500_config.int_gpio_num = -1;   // Không dùng ngắt (nếu ETH_PIN_INT = -1)
    w5500_config.poll_period_ms = 10;

     // Tạo MAC và PHY
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    if (mac == NULL) {
        ESP_LOGE("ETH", "Failed to create MAC");
        return NULL;
    }
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    if (phy == NULL) {
        ESP_LOGE("ETH", "Failed to create PHY");
        return NULL;
    }

    // Cấu hình driver Ethernet
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    esp_err_t ret = esp_eth_driver_install(&config, &eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("ETH", "Failed to install Ethernet driver");
        return NULL;
    }

    // Thiết lập địa chỉ MAC (nếu cần)
    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, (uint8_t[]) {
        0x00, 0x08, 0xdc, 0xab, 0xcd, 0xef
    }));

    return eth_handle;
}


void ethernet_w5500_init(void) {
    ESP_LOGI(TAG, "Initializing W5500 Ethernet...");

    // Cấu hình Ethernet
    esp_netif_config_t eth_cfg = ESP_NETIF_DEFAULT_ETH();
    eth_netif = esp_netif_new(&eth_cfg);
    
        // Cấu hình SPI
    spi_device_handle_t spi_handle = configure_spi();
        // Cấu hình Ethernet driver
    eth_handle = configure_ethernet(spi_handle);
    
    // Gắn Ethernet driver với TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

}
