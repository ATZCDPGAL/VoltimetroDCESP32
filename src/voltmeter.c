#include "voltmeter.h"
#include "config.h"
#include <math.h>
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "VOLTMETER";

static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t cali_handle = NULL;
static bool s_has_cali = false;

/* Variables para filtrado y estadísticas */
static float s_filtered_vin = 0.0f;
static float s_filtered_vadc_mv = 0.0f;
static bool  s_first_reading = true;
static float s_v_min = 999.0f;
static float s_v_max = 0.0f;

/* Relación del divisor de voltaje K = (R1 + R2) / R2 */
static const float s_divider_ratio = (VOLTMETER_R1_OHMS + VOLTMETER_R2_OHMS) / VOLTMETER_R2_OHMS;

esp_err_t voltmeter_init(void) {
    ESP_LOGI(TAG, "Inicializando ADC1 en GPIO %d (ADC_CH%d)...", 34, VOLTMETER_ADC_CHANNEL);
    ESP_LOGI(TAG, "Divisor R1=%.1f kΩ, R2=%.1f kΩ -> Relación K=%.4f",
             VOLTMETER_R1_OHMS / 1000.0f, VOLTMETER_R2_OHMS / 1000.0f, s_divider_ratio);

    // 1. Inicializar Unidad ADC
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = VOLTMETER_ADC_UNIT,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al crear unidad ADC1: %d", ret);
        return ret;
    }

    // 2. Configurar canal con atenuación de 12dB (rango hasta ~3.1V)
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ret = adc_oneshot_config_channel(adc_handle, VOLTMETER_ADC_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error configurando canal ADC: %d", ret);
        return ret;
    }

    // 3. Calibración por hardware eFuse
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = VOLTMETER_ADC_UNIT,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle);
    if (ret == ESP_OK) {
        s_has_cali = true;
        ESP_LOGI(TAG, "Calibración eFuse Line Fitting habilitada con éxito.");
    } else {
        ESP_LOGW(TAG, "eFuse Line Fitting no disponible, usando cálculo por software.");
    }
#elif ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = VOLTMETER_ADC_UNIT,
        .chan = VOLTMETER_ADC_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);
    if (ret == ESP_OK) {
        s_has_cali = true;
        ESP_LOGI(TAG, "Calibración eFuse Curve Fitting habilitada con éxito.");
    }
#endif

    voltmeter_reset_stats();
    return ESP_OK;
}

void voltmeter_reset_stats(void) {
    s_v_min = 999.0f;
    s_v_max = 0.0f;
    s_first_reading = true;
}

esp_err_t voltmeter_read(voltmeter_data_t *data) {
    if (!data || !adc_handle) return ESP_ERR_INVALID_ARG;

    uint32_t sum_mv = 0;
    int raw_val = 0;
    int mv_val = 0;

    // Sobremuestreo para eliminar ruido térmico y de conmutación
    for (uint16_t i = 0; i < VOLTMETER_SAMPLES_COUNT; i++) {
        esp_err_t ret = adc_oneshot_read(adc_handle, VOLTMETER_ADC_CHANNEL, &raw_val);
        if (ret != ESP_OK) {
            return ret;
        }

        if (s_has_cali) {
            adc_cali_raw_to_voltage(cali_handle, raw_val, &mv_val);
        } else {
            // Conversión lineal para 12dB (0-3100 mV)
            mv_val = (raw_val * 3100) / 4095;
        }
        sum_mv += mv_val;
    }

    float current_vadc_mv = (float)sum_mv / (float)VOLTMETER_SAMPLES_COUNT;

    // Cálculo del voltaje en la entrada antes del divisor:
    // Vin = (V_adc / 1000) * K * Factor_Calibración
    float raw_vin = (current_vadc_mv / 1000.0f) * s_divider_ratio * VOLTMETER_CALIB_FACTOR;

    // Supresión de offset cercano a 0V por no linealidad natural de entrada del ESP32
    if (raw_vin < VOLTMETER_ZERO_THRESHOLD) {
        raw_vin = 0.0f;
    }

    // Filtrado de respuesta suave Exponential Moving Average (EMA)
    if (s_first_reading) {
        s_filtered_vin = raw_vin;
        s_filtered_vadc_mv = current_vadc_mv;
        s_first_reading = false;
        s_v_min = raw_vin;
        s_v_max = raw_vin;
    } else {
        s_filtered_vin = (VOLTMETER_EMA_ALPHA * raw_vin) + ((1.0f - VOLTMETER_EMA_ALPHA) * s_filtered_vin);
        s_filtered_vadc_mv = (VOLTMETER_EMA_ALPHA * current_vadc_mv) + ((1.0f - VOLTMETER_EMA_ALPHA) * s_filtered_vadc_mv);
    }

    // Evitar valores negativos por artefactos numéricos
    if (s_filtered_vin < 0.0f) {
        s_filtered_vin = 0.0f;
    }

    // Actualización de Peak Hold (mínimo y máximo)
    if (s_filtered_vin > s_v_max) {
        s_v_max = s_filtered_vin;
    }
    if (s_filtered_vin < s_v_min) {
        s_v_min = s_filtered_vin;
    }

    // Detección de sobrevoltaje
    bool is_overload = (s_filtered_vin > VOLTAGE_OVERLOAD_LIMIT) || (current_vadc_mv >= 3100.0f);

    // Asignación de salida
    data->v_in = s_filtered_vin;
    data->v_adc_mv = s_filtered_vadc_mv;
    data->v_min = s_v_min;
    data->v_max = s_v_max;
    data->overload = is_overload;

    return ESP_OK;
}
