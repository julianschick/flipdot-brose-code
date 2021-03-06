#include "libs/nvs.h"
#include "libs/blue.h"
#include "libs/mdns.h"
#include "libs/wifi.h"
#include "libs/tcp_server.h"

#define PIN_NUM_MOSI GPIO_NUM_13
#define PIN_NUM_CLK  GPIO_NUM_14

#define PIN_NUM_CLR GPIO_NUM_19
#define PIN_NUM_RCLK_SEL GPIO_NUM_18
#define PIN_NUM_RCLK_CONF GPIO_NUM_21
#define PIN_NUM_OE_SEL GPIO_NUM_5
#define PIN_NUM_OE_CONF GPIO_NUM_15

#define TAG "startup"
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include <esp_log.h>

WS2812Controller* led_ctrl;
WS2812Buffer* ledBuffer;
FlipdotBuffer* flipdotBuffer;

TaskHandle_t ledTask;
TaskHandle_t tcpServerTask;
TaskHandle_t flipdotTask;

inline void safety_init() {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (uint64_t) 1 << (uint64_t) PIN_NUM_OE_CONF;;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(PIN_NUM_OE_CONF, 1);
}

void led_task(void* params) {

    led_ctrl->readState();
    TickType_t lastStateSave = xTaskGetTickCount();

    while(1) {

        ledBuffer->executeNext(1000);

        if ((xTaskGetTickCount() - lastStateSave)*portTICK_PERIOD_MS > 20*1000) {
            if (led_ctrl->saveStateIfNecessary()) {
                lastStateSave = xTaskGetTickCount();
            }
        }
    }

}

void flipdot_task(void* params) {
    while(1) {
        flipdotBuffer->executeNext(1000);
    }
}

extern "C" void app_main() {

    gpio_set_level(PIN_NUM_OE_CONF, 1);
    safety_init();

    ESP_LOGI(TAG, "Safety init done");

    flipdot_driver_pins_t pins;
    pins.ser = PIN_NUM_MOSI;
    pins.serclk = PIN_NUM_CLK;
    pins.clr = PIN_NUM_CLR;
    pins.rclk_conf = PIN_NUM_RCLK_CONF;
    pins.rclk_sel = PIN_NUM_RCLK_SEL;
    pins.oe_conf = PIN_NUM_OE_CONF;
    pins.oe_sel = PIN_NUM_OE_SEL;

    flipdot_driver_timing_config_t timing;
    timing.set_usecs = 200;
    timing.reset_usecs = 200;

    FlipdotDriver *drv = new FlipdotDriver(28, 16, 4, &pins, &timing);
    ESP_LOGI(TAG, "Flipdot driver initialized");

    uint32_t short_period = 28;
    uint32_t long_period = 72;
    ws2812_timing_config_t timing_config = {short_period, long_period, long_period, short_period};
    WS2812Driver* led_drv = new WS2812Driver(GPIO_NUM_32, RMT_CHANNEL_0, timing_config, 36);
    ESP_LOGI(TAG, "LED driver initialized");

    FlipdotDisplay* dsp = new FlipdotDisplay(drv);
    led_ctrl = new WS2812Controller(led_drv);
    led_drv->clear();
    ledBuffer = new WS2812Buffer(led_ctrl, 10);
    flipdotBuffer = new FlipdotBuffer(dsp, 10);
    ESP_LOGI(TAG, "Controllers initialized");

    Nvs::setup();

    // general networking pre-setup (main work is done in Wifi::setup())
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    start_mdns_service();
    Wifi::setup();
    Blue::setup();

    ESP_LOGI(TAG, "Network services up");

    ESP_LOGI(TAG, "Starting TCP server task...");
    xTaskCreatePinnedToCore(tcp_server_task, "tcp-server-task", 2600, NULL, configMAX_PRIORITIES - 2, &tcpServerTask, 0);

    ESP_LOGI(TAG, "Starting LED task...");
    xTaskCreatePinnedToCore(led_task, "led-task", 1800, NULL, configMAX_PRIORITIES - 1, &ledTask, 0);

    ESP_LOGI(TAG, "Starting flipdot task...");
    xTaskCreatePinnedToCore(flipdot_task, "flipdot-task", 2000, NULL, configMAX_PRIORITIES - 1, &flipdotTask, 1);

    ESP_LOGI(TAG, "Init sequence done");
}
