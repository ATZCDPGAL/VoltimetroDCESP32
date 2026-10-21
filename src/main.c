#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config.h"
#include "st7735.h"
#include "voltmeter.h"
#include "ui.h"

static const char *TAG = "MAIN_APP";

void app_main(void) {
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "   VOLTIMETRO DC 0-20V (ESP32 + DIVISOR + TFT)   ");
    ESP_LOGI(TAG, "=================================================");

    /* 1. Inicializar pantalla TFT ST7735 (128x160) */
    esp_err_t ret = st7735_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error crítico al inicializar display TFT: %d", ret);
        return;
    }
    ESP_LOGI(TAG, "Display TFT inicializado correctamente.");

    /* 2. Pantalla de presentación (Splash Screen) */
    ui_show_splash();

    /* 3. Inicializar canal ADC1 y calibración eFuse del voltímetro */
    ret = voltmeter_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error crítico al inicializar ADC del voltímetro: %d", ret);
        return;
    }
    ESP_LOGI(TAG, "Módulo analógico ADC del voltímetro listo.");

    vTaskDelay(pdMS_TO_TICKS(400));

    /* 4. Dibujar la interfaz de medición en el display */
    ui_init();

    /* 5. Bucle principal de adquisición y visualización */
    voltmeter_data_t vdata;
    uint32_t log_counter = 0;

    ESP_LOGI(TAG, "Iniciando ciclo de medición continua (10 Hz)...");

    while (1) {
        // Leer voltaje con sobremuestreo (64 muestras) y filtrado EMA
        ret = voltmeter_read(&vdata);
        if (ret == ESP_OK) {
            // Actualizar interfaz gráfica sin flicker
            ui_update(&vdata);

            // Reportar telemetría por puerto serie cada 500 ms (5 ciclos x 100 ms)
            if (++log_counter >= 5) {
                log_counter = 0;
                ESP_LOGI(TAG, "[LECTURA] Vin: %5.2f V | Vadc: %4.0f mV | Min: %4.1f V | Max: %4.1f V %s",
                         vdata.v_in,
                         vdata.v_adc_mv,
                         vdata.v_min,
                         vdata.v_max,
                         vdata.overload ? "[! SOBREVOLTAJE !]" : "[NORMAL]");
            }
        } else {
            ESP_LOGW(TAG, "Error al leer ADC: %d", ret);
        }

        // Tasa de refresco: 100 ms (10 lecturas/segundo)
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}