#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdint.h>
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

/* =========================================================================
 * PINES DE CONEXIÓN DISPLAY TFT 128*160 RGB VER 1 (ST7735)
 * ========================================================================= */
#define PIN_TFT_SCL         GPIO_NUM_18   // SPI Clock (SCL / SCK)
#define PIN_TFT_SDA         GPIO_NUM_23   // SPI MOSI / Data (SDA / MOSI)
#define PIN_TFT_CS          GPIO_NUM_5    // Chip Select (CS / SS)
#define PIN_TFT_DC          GPIO_NUM_21   // Data/Command (DC / A0 / RS)
#define PIN_TFT_RST         GPIO_NUM_22   // Reset (RES / RST)
#define PIN_TFT_BLK         GPIO_NUM_15   // Backlight (BLK / LED) - Opcional

/* Dimensiones del Display (128x160) */
#define TFT_WIDTH           128
#define TFT_HEIGHT          160

/* Offsets para controlador ST7735 */
#define ST7735_X_OFFSET     0
#define ST7735_Y_OFFSET     0

/* =========================================================================
 * CONFIGURACIÓN DEL VOLTÍMETRO DC Y DIVISOR DE VOLTAJE
 * =========================================================================
 * Circuito Divisor de Voltaje:
 *
 *   Vin (+) [0-20V] -----[ R1: 100 kΩ ]-----+-----> ESP32 GPIO 34 (ADC1_CH6)
 *                                           |
 *                                     [ R2: 15 kΩ ]
 *                                           |
 *   GND ------------------------------------+-----> ESP32 GND
 *
 * Relación de división teórica:
 *   K = (R1 + R2) / R2 = (100k + 15k) / 15k = 115 / 15 = 7.6667
 *
 * Voltaje en GPIO34 a fondo de escala (20.0 V):
 *   V_adc = 20.0 V / 7.6667 ≈ 2.609 V (dentro del rango seguro de 0 - 3.1 V con atenuación de 12dB)
 * ========================================================================= */

/* Pin ADC (GPIO34 = ADC1_CH6, entrada dedicada y de bajo ruido) */
#define VOLTMETER_ADC_UNIT        ADC_UNIT_1
#define VOLTMETER_ADC_CHANNEL     ADC_CHANNEL_6 // GPIO 34

/* Valores de las resistencias del divisor de voltaje (en Ohmios) */
#define VOLTMETER_R1_OHMS         100000.0f     // R1 = 100 kΩ (Conectado a Vin+)
#define VOLTMETER_R2_OHMS         15000.0f      // R2 = 15 kΩ  (Conectado a GND)

/* Factor de calibración fina (trimming) para compensar tolerancia de resistencias */
#define VOLTMETER_CALIB_FACTOR    1.000f

/* Rango y límites de voltaje */
#define VOLTAGE_MIN_RANGE         0.0f          // 0 Voltios
#define VOLTAGE_MAX_RANGE         20.0f         // 20 Voltios (Rango nominal)
#define VOLTAGE_WARNING_LIMIT     18.0f         // Umbral de alerta visual amarilla
#define VOLTAGE_OVERLOAD_LIMIT    20.0f         // Umbral de sobrevoltaje roja

/* Parámetros de sobremuestreo y filtrado */
#define VOLTMETER_SAMPLES_COUNT   64            // Muestras promediadas por lectura
#define VOLTMETER_EMA_ALPHA       0.20f         // Factor de suavizado exponencial (0.01 - 1.0)
#define VOLTMETER_ZERO_THRESHOLD  0.06f         // Supresión de ruido cerca de 0V (60 mV)

#endif /* CONFIG_H_ */
