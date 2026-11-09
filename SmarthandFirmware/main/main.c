#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "SmarthandFirmware";

void app_main(void)
{
    ESP_LOGI(TAG, "SmarthandFirmware 启动，主控：%s", CONFIG_IDF_TARGET);
}
