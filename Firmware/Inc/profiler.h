
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __PROFILER_H__
#define __PROFILER_H__

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private define ------------------------------------------------------------*/
void profiler_systick_handler(void);
void profiler_parse_command(uint8_t* command, uint16_t length);
void profiler_init(void);
void profiler_handler(void);
void profiler_start_adc(void);

#endif /* __PROFILER_H__ */
