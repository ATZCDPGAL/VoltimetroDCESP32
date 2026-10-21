#ifndef UI_H_
#define UI_H_

#include "voltmeter.h"

/**
 * @brief Muestra la pantalla de inicio (Splash Screen) con información del instrumento
 */
void ui_show_splash(void);

/**
 * @brief Inicializa la interfaz gráfica base en el display TFT (128x160)
 */
void ui_init(void);

/**
 * @brief Actualiza la pantalla de forma diferencial y libre de parpadeo (flicker-free)
 * @param data Puntero a los datos actualizados del voltímetro
 */
void ui_update(const voltmeter_data_t *data);

#endif /* UI_H_ */
