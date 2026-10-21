#include "ui.h"
#include "st7735.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define CARD_X       4
#define CARD_Y       24
#define CARD_W       120
#define CARD_H       46

#define BAR_X        14
#define BAR_Y        76
#define BAR_MAX_W    100
#define BAR_H        10

/* Variables de estado anterior para renderizado diferencial sin flicker */
static float s_last_vin = -999.0f;
static float s_last_vadc = -999.0f;
static float s_last_min = -999.0f;
static float s_last_max = -999.0f;
static int   s_last_bar_w = 0;
static bool  s_last_overload = false;
static int   s_last_status_type = -1; // 0=Normal, 1=Alerta, 2=Sobrecarga

/* Pantalla de bienvenida (Splash Screen) */
void ui_show_splash(void) {
    st7735_fill_screen(ST7735_BLACK);

    // Cabecera elegante
    st7735_fill_rect(0, 0, TFT_WIDTH, 22, 0x0841);
    st7735_draw_string(24, 6, "VOLTIMETRO DC", ST7735_CYAN, 0x0841, 1);
    st7735_draw_fast_h_line(0, 22, TFT_WIDTH, ST7735_CYAN);

    // Información del instrumento
    st7735_draw_string(18, 42, "ESP32 + ST7735", ST7735_WHITE, ST7735_BLACK, 1);
    st7735_draw_string(18, 58, "Rango: 0 - 20 V", ST7735_YELLOW, ST7735_BLACK, 1);
    st7735_draw_string(18, 74, "Divisor: 100k/15k", ST7735_LIGHTGREY, ST7735_BLACK, 1);
    st7735_draw_string(18, 90, "TFT 128x160 SPI", ST7735_DARKCYAN, ST7735_BLACK, 1);

    st7735_draw_string(22, 114, "Iniciando ADC...", ST7735_DARKGREY, ST7735_BLACK, 1);

    // Barra de progreso de inicio
    st7735_draw_rect(14, 130, 100, 10, ST7735_DARKCYAN);
    for (int p = 0; p <= 96; p += 8) {
        st7735_fill_rect(16, 132, p, 6, ST7735_CYAN);
        vTaskDelay(pdMS_TO_TICKS(60));
    }
}

/* Inicialización del fondo y elementos estáticos */
void ui_init(void) {
    st7735_fill_screen(ST7735_BLACK);

    // 1. Cabecera principal (0 a 22 px)
    st7735_fill_rect(0, 0, TFT_WIDTH, 22, 0x0841);
    st7735_draw_string(22, 4, "DC VOLTMETER", ST7735_CYAN, 0x0841, 1);
    st7735_draw_string(42, 14, "0 - 20 V", ST7735_YELLOW, 0x0841, 1);
    st7735_draw_fast_h_line(0, 22, TFT_WIDTH, ST7735_DARKCYAN);

    // 2. Tarjeta del valor principal de voltaje
    st7735_fill_rect(CARD_X, CARD_Y, CARD_W, CARD_H, ST7735_CARD_BG);
    st7735_draw_rect(CARD_X, CARD_Y, CARD_W, CARD_H, ST7735_DARKGREY);

    // Símbolo de unidad 'V' fijo
    st7735_draw_string(104, 40, "V", ST7735_YELLOW, ST7735_CARD_BG, 2);

    // 3. Barra analógica de progreso (0 a 20V)
    st7735_draw_rect(BAR_X - 2, BAR_Y - 2, BAR_MAX_W + 4, BAR_H + 4, ST7735_LIGHTGREY);

    // Marcas de escala (Ticks: 0, 5, 10, 15, 20V)
    for (int i = 0; i <= 4; i++) {
        int tick_x = BAR_X + (i * BAR_MAX_W) / 4;
        st7735_draw_fast_v_line(tick_x, BAR_Y + BAR_H + 2, 4, ST7735_LIGHTGREY);
    }

    // Leyendas numéricas de la escala
    st7735_draw_string(BAR_X - 2, 92, "0", ST7735_DARKGREY, ST7735_BLACK, 1);
    st7735_draw_string(BAR_X + 22, 92, "5", ST7735_DARKGREY, ST7735_BLACK, 1);
    st7735_draw_string(BAR_X + 44, 92, "10", ST7735_DARKGREY, ST7735_BLACK, 1);
    st7735_draw_string(BAR_X + 69, 92, "15", ST7735_DARKGREY, ST7735_BLACK, 1);
    st7735_draw_string(BAR_X + 90, 92, "20V", ST7735_YELLOW, ST7735_BLACK, 1);

    // Línea divisora entre barra y telemetría
    st7735_draw_fast_h_line(4, 104, TFT_WIDTH - 8, ST7735_DARKGREY);

    // Reset de tracking para forzar dibujo completo en primera actualización
    s_last_vin = -999.0f;
    s_last_vadc = -999.0f;
    s_last_min = -999.0f;
    s_last_max = -999.0f;
    s_last_bar_w = 0;
    s_last_overload = false;
    s_last_status_type = -1;
}

/* Actualización diferencial (libre de parpadeos / anti-flicker) */
void ui_update(const voltmeter_data_t *data) {
    if (!data) return;

    // 1. Actualizar el valor numérico grande de Voltaje (Tamaño 3)
    if (fabsf(data->v_in - s_last_vin) >= 0.01f || data->overload != s_last_overload) {
        s_last_vin = data->v_in;
        s_last_overload = data->overload;

        char buf_v[16];
        snprintf(buf_v, sizeof(buf_v), "%5.2f", data->v_in);

        // Color condicional según nivel de voltaje
        uint16_t num_color = ST7735_GREEN;
        if (data->overload || data->v_in > VOLTAGE_OVERLOAD_LIMIT) {
            num_color = ST7735_RED;
        } else if (data->v_in >= VOLTAGE_WARNING_LIMIT) {
            num_color = ST7735_YELLOW;
        }

        // Renderizado con fondo sólido en la tarjeta para evitar flicker
        st7735_draw_string(10, 35, buf_v, num_color, ST7735_CARD_BG, 3);
    }

    // 2. Barra analógica de 0 a 20V
    int target_w = (int)((data->v_in / VOLTAGE_MAX_RANGE) * (float)BAR_MAX_W);
    if (target_w > BAR_MAX_W) target_w = BAR_MAX_W;
    if (target_w < 0) target_w = 0;

    if (target_w != s_last_bar_w || data->overload != s_last_overload) {
        uint16_t bar_color = ST7735_GREEN;
        if (data->overload || data->v_in > VOLTAGE_OVERLOAD_LIMIT) {
            bar_color = ST7735_RED;
        } else if (data->v_in >= VOLTAGE_WARNING_LIMIT) {
            bar_color = ST7735_YELLOW;
        }

        if (target_w > s_last_bar_w) {
            // Extender barra
            st7735_fill_rect(BAR_X + s_last_bar_w, BAR_Y, target_w - s_last_bar_w, BAR_H, bar_color);
            // Repintar porción anterior si cambió color general
            if (bar_color != ST7735_GREEN && s_last_bar_w > 0) {
                st7735_fill_rect(BAR_X, BAR_Y, s_last_bar_w, BAR_H, bar_color);
            }
        } else {
            // Reducir barra (borrar segmento sobrante con fondo negro)
            st7735_fill_rect(BAR_X + target_w, BAR_Y, s_last_bar_w - target_w, BAR_H, ST7735_BLACK);
            // Asegurar color uniforme en la porción activa
            if (target_w > 0) {
                st7735_fill_rect(BAR_X, BAR_Y, target_w, BAR_H, bar_color);
            }
        }
        s_last_bar_w = target_w;
    }

    // 3. Telemetría y diagnóstico en tiempo real
    if (fabsf(data->v_adc_mv - s_last_vadc) >= 2.0f) {
        s_last_vadc = data->v_adc_mv;
        char buf_adc[24];
        snprintf(buf_adc, sizeof(buf_adc), "Vad:  %4.0f mV     ", data->v_adc_mv);
        st7735_draw_string(8, 108, buf_adc, ST7735_LIGHTGREY, ST7735_BLACK, 1);
    }

    // Estadísticas Min / Max
    if (fabsf(data->v_min - s_last_min) >= 0.05f || fabsf(data->v_max - s_last_max) >= 0.05f) {
        s_last_min = data->v_min;
        s_last_max = data->v_max;
        char buf_stats[24];
        snprintf(buf_stats, sizeof(buf_stats), "Min:%4.1fV Max:%4.1fV", data->v_min, data->v_max);
        st7735_draw_string(8, 120, buf_stats, ST7735_DARKCYAN, ST7735_BLACK, 1);
    }

    // Línea de configuración del divisor
    static bool s_div_drawn = false;
    if (!s_div_drawn) {
        char buf_div[24];
        float k = (VOLTMETER_R1_OHMS + VOLTMETER_R2_OHMS) / VOLTMETER_R2_OHMS;
        snprintf(buf_div, sizeof(buf_div), "Kdiv: %4.2fx (100k/15k)", k);
        st7735_draw_string(8, 132, buf_div, ST7735_DARKGREY, ST7735_BLACK, 1);
        s_div_drawn = true;
    }

    // 4. Badge de estado del instrumento
    int current_status_type = 0;
    if (data->overload || data->v_in > VOLTAGE_OVERLOAD_LIMIT) {
        current_status_type = 2; // Sobrecarga
    } else if (data->v_in >= VOLTAGE_WARNING_LIMIT) {
        current_status_type = 1; // Alerta
    }

    if (current_status_type != s_last_status_type) {
        s_last_status_type = current_status_type;
        if (current_status_type == 2) {
            st7735_draw_string(8, 146, "! SOBREVOLTAJE >20V !", ST7735_RED, ST7735_BLACK, 1);
        } else if (current_status_type == 1) {
            st7735_draw_string(8, 146, "[ ALERTA: ELEVADO  ] ", ST7735_YELLOW, ST7735_BLACK, 1);
        } else {
            st7735_draw_string(8, 146, "[ ESTADO: NORMAL   ] ", ST7735_GREEN, ST7735_BLACK, 1);
        }
    }
}
