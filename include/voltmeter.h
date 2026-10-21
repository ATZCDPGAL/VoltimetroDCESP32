#ifndef VOLTMETER_H_
#define VOLTMETER_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* Estructura con datos de medición del voltímetro */
typedef struct {
    float v_in;           // Voltaje de entrada medido (en Voltios, 0.00 a 20.00V)
    float v_adc_mv;       // Voltaje presente en el pin GPIO del ESP32 (en mV)
    float v_min;          // Voltaje mínimo registrado (Peak Hold Min)
    float v_max;          // Voltaje máximo registrado (Peak Hold Max)
    bool  overload;       // Indica si el voltaje sobrepasa el límite seguro (> 20.0V)
} voltmeter_data_t;

/**
 * @brief Inicializa el canal ADC1 y la calibración eFuse (Line Fitting / Curve Fitting)
 * @return ESP_OK en éxito, o código de error correspondiente.
 */
esp_err_t voltmeter_init(void);

/**
 * @brief Realiza una lectura del voltímetro con sobremuestreo y filtrado exponencial
 * @param data Puntero a estructura donde se almacenarán las lecturas
 * @return ESP_OK en éxito, o código de error
 */
esp_err_t voltmeter_read(voltmeter_data_t *data);

/**
 * @brief Reinicia los registros de valores mínimos y máximos (Peak Hold)
 */
void voltmeter_reset_stats(void);

#endif /* VOLTMETER_H_ */
