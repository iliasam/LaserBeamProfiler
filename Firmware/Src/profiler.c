/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "profiler.h"
#include "stm32f1xx_hal.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"

/* Private define ------------------------------------------------------------*/
#define PROFILER_GPIO                   GPIOB
#define PROFILER_STEP_PIN               GPIO_PIN_0
#define PROFILER_DIR_PIN                GPIO_PIN_1
#define PROFILER_EN_PIN                 GPIO_PIN_11

#define PROFILER_LED_PIN                LED_Pin
#define PROFILER_LED_GPIO               LED_GPIO_Port

#define PROFILER_MAX_POS                (400*5)

//Size in uint16_t words
#define PROFILER_HEADER_SIZE            (2)

#define PROFILER_TX_SIZE                ((PROFILER_MAX_POS + PROFILER_HEADER_SIZE) * 2)

#define PROFILER_ADC_BURST_CNT          50

typedef enum
{
  PROFILER_CAPTURE_IDLE,
  PROFILER_CAPTURE_GO_TO_ZERO,
  PROFILER_CAPTURE_PROCESS,
  PROFILER_CAPTURE_DONE,
  PROFILER_CAPTURE_GO_TO_ZERO2,
} profiler_capture_state_t;


/* Private variables ---------------------------------------------------------*/
extern ADC_HandleTypeDef hadc1;

//Position in steps
int32_t profiler_position = 0;
int32_t profiler_target_position = 0;

uint8_t profiler_step_polarity = 0;

uint8_t profiler_need_send_data_flag = 0;

uint8_t profiler_capture_enabled_flag = 0;

//Captured data
uint16_t profiler_captured_data[PROFILER_MAX_POS + PROFILER_HEADER_SIZE];

profiler_capture_state_t profiler_capture_state = PROFILER_CAPTURE_IDLE;

volatile uint8_t profiler_adc_count = 0;
volatile uint32_t profiler_burst_integrator = 0;
volatile uint8_t profiler_adc_done = 0;

uint8_t profiler_adc_test_flag = 0;

/* Private function prototypes -----------------------------------------------*/
void profiler_set_direction(uint8_t dir);
void profiler_send_results(void);
void profiler_start_adc(void);
void profiler_save_captured_point(void);
void profiler_do_adc_test(void);


void profiler_init(void)
{
  profiler_captured_data[0] = 0xBBAA;
  profiler_captured_data[1] = 0xDDCC;
  
  for (uint16_t i = 0; i < PROFILER_MAX_POS; i++)
  {
    profiler_captured_data[i + PROFILER_HEADER_SIZE] = i * 10;
  }
}


void profiler_systick_handler(void)
{
  int32_t pos_error = profiler_target_position - profiler_position;
  
  if (abs(pos_error) != 0)
  {
    HAL_GPIO_WritePin(PROFILER_LED_GPIO, PROFILER_LED_PIN, GPIO_PIN_RESET);//inversion
    HAL_GPIO_WritePin(PROFILER_GPIO, PROFILER_EN_PIN, GPIO_PIN_RESET);//inversion
    
    profiler_step_polarity^= 1;

    //front edge
    if (profiler_step_polarity)
    {
      //movement is needed
      if (pos_error > 0)
      {
        if (profiler_capture_state == PROFILER_CAPTURE_PROCESS)
          profiler_start_adc();
        
        profiler_set_direction(0);
        profiler_position++;
      }
      else
      {
        profiler_set_direction(1);
        profiler_position--;
      }
    }
    else
    {
      profiler_save_captured_point();
    }
    
    //Step command
    HAL_GPIO_WritePin(
      PROFILER_GPIO, PROFILER_STEP_PIN, (GPIO_PinState)profiler_step_polarity);

  }
  else
  {
    HAL_GPIO_WritePin(PROFILER_LED_GPIO, PROFILER_LED_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(PROFILER_GPIO, PROFILER_EN_PIN, GPIO_PIN_SET);//inversion
  }
}

void profiler_save_captured_point(void)
{
  if (profiler_capture_state == PROFILER_CAPTURE_PROCESS)
  {
    if ((profiler_position >= 0) && (profiler_position < PROFILER_MAX_POS))
    {
      if (profiler_adc_done == 1)
      {
        profiler_captured_data[PROFILER_HEADER_SIZE + profiler_position] = 
          profiler_burst_integrator / PROFILER_ADC_BURST_CNT;
      }
    }
  }
}


void profiler_handler(void)
{
  if (profiler_need_send_data_flag)
  {
    profiler_need_send_data_flag = 0;
    profiler_send_results();
  }
  
  if (profiler_adc_test_flag)
  {
    profiler_adc_test_flag = 0;
    profiler_do_adc_test();
  }
  
  switch (profiler_capture_state)
  {
    case PROFILER_CAPTURE_GO_TO_ZERO:
      if (profiler_position == 0)
      {
        profiler_capture_state = PROFILER_CAPTURE_PROCESS;
        profiler_target_position = PROFILER_MAX_POS;//open knife
      }
      break;
      
    case PROFILER_CAPTURE_PROCESS:
      if (profiler_position == PROFILER_MAX_POS)
      {
        profiler_capture_state = PROFILER_CAPTURE_GO_TO_ZERO2;
        profiler_need_send_data_flag = 1;
        profiler_target_position = 0;//close knife
      }
      break;
      
    case PROFILER_CAPTURE_GO_TO_ZERO2:
      if (profiler_position == 0)
      {
        profiler_capture_state = PROFILER_CAPTURE_IDLE;
      }
      break;
      
    default: break;
  }
}

//Capture all data while motor is stopped
void profiler_do_adc_test(void)
{
  uint16_t i;
  for (i = 0; i < PROFILER_MAX_POS; i++)
  {
    profiler_start_adc();
    while (profiler_adc_done == 0) //wait for burst done
    {
    }
    profiler_captured_data[PROFILER_HEADER_SIZE + i] = 
          profiler_burst_integrator / PROFILER_ADC_BURST_CNT;
  }
  profiler_need_send_data_flag = 1;
}

void profiler_set_direction(uint8_t dir)
{
  if (dir)
    HAL_GPIO_WritePin(PROFILER_GPIO, PROFILER_DIR_PIN, GPIO_PIN_SET);
  else
    HAL_GPIO_WritePin(PROFILER_GPIO, PROFILER_DIR_PIN, GPIO_PIN_RESET);
}

void profiler_parse_command(uint8_t* command, uint16_t length)
{
  if (length == 1)
  {
    switch (command[0])
    {
      case 'C':
        profiler_target_position = 0;
        break;
      
      case 'O':
        profiler_target_position = PROFILER_MAX_POS;
        break;
        
      case 'L':
        profiler_target_position = -50000;//left
        break;
        
      case 'R':
        profiler_target_position = 50000;//right
        break;
        
      case 'Z':
        //Set zero and stop
        profiler_target_position = 0;
        profiler_position = 0;
        break;
        
      case 'S':
        profiler_need_send_data_flag = 1;
        break;
        
      case 'M'://run capture
        profiler_capture_state = PROFILER_CAPTURE_GO_TO_ZERO;
        profiler_target_position = 0;
        break;
        
      case 'T'://adc test
        profiler_capture_state = PROFILER_CAPTURE_IDLE;
        profiler_adc_test_flag = 1;
        break;
      
      default: break;
    }
  }
}

//ADC interrupt - replace weak function in HAL
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc1)
{
  uint16_t adc_result = HAL_ADC_GetValue(hadc1);
  if (profiler_adc_count < (PROFILER_ADC_BURST_CNT - 1))
  {
    profiler_burst_integrator+= (uint32_t)adc_result;
    profiler_adc_count++;
    HAL_ADC_Start_IT(hadc1);
  }
  else
  {
    profiler_adc_done = 1;
  }
}

void profiler_start_adc(void)
{
  profiler_adc_count = 0;
  profiler_burst_integrator = 0;
  profiler_adc_done = 0;
  HAL_ADC_Start_IT(&hadc1);
}

void profiler_send_results(void)
{
  //CDC_write(profiler_captured_data, PROFILER_TX_SIZE);
  cdc_tx_big_packet((uint8_t*)&profiler_captured_data[0], PROFILER_TX_SIZE);
}

