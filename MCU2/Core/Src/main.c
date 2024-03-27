// PA0(GPIOA0 GPIO_INPUT) ky038 detected sound change
// D2(GPIOA 10 INPUT) Capteur infrarouge D2+GND+VCC(3.3V)
#include "main.h"
#include <string.h>

#define MAX_MORSE_LENGTH 6  // 摩尔斯电码单个字符的最大长度
#define SOUND_THRESHOLD 1000  // 声音检测阈值
#define DASH_THRESHOLD 100  // 划的最小持续时间阈值，设置为100毫秒
#define CHARACTER_SPACING_THRESHOLD 1000  // 字符间隔时间阈值，设置为1000毫秒
#define FILTER_SIZE 10  // 滑动平均滤波器的大小
#define DEBOUNCE_TIME 50  // 去抖动时间阈值，单位为毫秒
#define PEAK_DETECTION_THRESHOLD 500  // 峰值检测阈值


#define IR_SENSOR_PIN GPIO_PIN_10
#define IR_SENSOR_GPIO_PORT GPIOA

ADC_HandleTypeDef hadc1;
UART_HandleTypeDef huart2;

volatile uint16_t adc_value_for_debug = 0;
uint16_t filter_buffer[FILTER_SIZE] = {0};
uint8_t filter_index = 0;
// 定义存储样本的数组和索引变量


#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */

PUTCHAR_PROTOTYPE {
    HAL_UART_Transmit(&huart2, (uint8_t*) &ch, 1, HAL_MAX_DELAY);
    return ch;
}

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
char MorseCodeToChar(const char* morseCode);

typedef enum {
    SIGNAL_SOURCE_SOUND,
    SIGNAL_SOURCE_IR
} SignalSource;

SignalSource last_signal_source = SIGNAL_SOURCE_SOUND; // 声明并初始化last_signal_source

uint16_t Read_ADC(void) {
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY) == HAL_OK) {
        adc_value_for_debug = HAL_ADC_GetValue(&hadc1);
        return adc_value_for_debug;
    }
    return 0;
}

void ProcessSignal(SignalSource source, uint32_t current_time, uint8_t signal_start) {
    static uint32_t signal_start_time = 0;
    static char morse_sequence[MAX_MORSE_LENGTH + 1] = {0};
    static int morse_index = 0;

    if (signal_start) {
        signal_start_time = current_time;  // 记录信号开始的时间
    } else {
        if (signal_start_time == 0) return;  // 如果之前没有记录开始时间，则忽略

        // 计算信号持续时间
        uint32_t signal_duration = current_time - signal_start_time;

        // 更新摩尔斯电码序列
        if (signal_duration < DASH_THRESHOLD) {
            morse_sequence[morse_index++] = '.';
        } else {
            morse_sequence[morse_index++] = '-';
        }

        // 如果摩尔斯电码序列已满或者信号源改变，尝试解析字符
        if (morse_index == MAX_MORSE_LENGTH || source != last_signal_source) {
            morse_sequence[morse_index] = '\0';  // 结束摩尔斯电码序列
            char translated_char = MorseCodeToChar(morse_sequence);  // 解析摩尔斯电码为字符

            if (translated_char != '\0') {
                HAL_UART_Transmit(&huart2, (uint8_t*)&translated_char, 1, HAL_MAX_DELAY);  // 通过UART发送解析出的字符
            }

            // 重置摩尔斯电码序列和索引
            memset(morse_sequence, 0, sizeof(morse_sequence));
            morse_index = 0;
        }

        signal_start_time = 0;  // 重置信号开始时间
    }
}

uint16_t Moving_Average_Filter(uint16_t new_sample) {
    filter_buffer[filter_index++] = new_sample;
    if (filter_index == FILTER_SIZE) {
        filter_index = 0;
    }

    uint32_t sum = 0;
    for (int i = 0; i < FILTER_SIZE; i++) {
        sum += filter_buffer[i];
    }

    return sum / FILTER_SIZE;
}


char MorseCodeToChar(const char* morseCode) {
    const char* MORSE_CODE_TABLE[36] = {
        ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",
        "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",
        "..-", "...-", ".--", "-..-", "-.--", "--..", "-----", ".----", "..---",
        "...--", "....-", ".....", "-....", "--...", "---..", "----."
    };

    for (int i = 0; i < 36; i++) {
        if (strcmp(morseCode, MORSE_CODE_TABLE[i]) == 0) {
            return i < 26 ? 'A' + i : '0' + (i - 26);
        }
    }
    return '\0';
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_ADC1_Init();

    uint32_t last_signal_time = 0;  // 用于去抖动
    uint32_t last_peak_time = 0;  // 用于峰值检测

    const char* welcome_msg = "UART is working!\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)welcome_msg, strlen(welcome_msg), HAL_MAX_DELAY);

    uint16_t adc_value;
    uint16_t filtered_value = 0;
    uint16_t display_value = 0;  // 用于SWV显示的值
    uint32_t signal_start = 0, signal_end = 0;
    char morse_sequence[MAX_MORSE_LENGTH + 1] = {0};
    int morse_index = 0;
    uint8_t last_display_value = 0;  // 记录上一次的display_value状态

    while (1) {
           adc_value = Read_ADC();
           filtered_value = Moving_Average_Filter(adc_value);
           uint32_t current_time = HAL_GetTick();

           // 检测声音并更新display_value
           display_value = filtered_value > SOUND_THRESHOLD ? 1 : 0;

           // 去抖动逻辑
           if ((current_time - last_signal_time) > DEBOUNCE_TIME) {
               // 峰值检测逻辑，声音开始
               if (display_value == 1 && last_display_value == 0 && (current_time - last_peak_time) > PEAK_DETECTION_THRESHOLD) {
                   last_peak_time = current_time;
                   signal_start = current_time;
               }

               // 声音结束，判断点或划
               if (display_value == 0 && last_display_value == 1) {
                   signal_end = current_time;
                   uint32_t signal_duration = signal_end - signal_start;

                   // 直接通过UART输出点或划
                   if (signal_duration < DASH_THRESHOLD) {
                       morse_sequence[morse_index++] = '.';
                       HAL_UART_Transmit(&huart2, (uint8_t *)".", 1, HAL_MAX_DELAY);
                   } else {
                       morse_sequence[morse_index++] = '-';
                       HAL_UART_Transmit(&huart2, (uint8_t *)"-", 1, HAL_MAX_DELAY);
                   }

                   last_signal_time = current_time;  // 更新去抖动时间
               }

               // 检测字符间隔或摩尔斯码序列已满，解析字符
               if ((signal_end != 0 && (current_time - signal_end) > CHARACTER_SPACING_THRESHOLD) || morse_index == MAX_MORSE_LENGTH) {
                   if (morse_index > 0) {
                       morse_sequence[morse_index] = '\0';  // 结束摩尔斯码序列

                       char translated_char = MorseCodeToChar(morse_sequence);  // 解析摩尔斯码为字符

                       if (translated_char != '\0') {
                           char msg[2] = {translated_char, '\0'};
                           HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);  // 通过UART发送解析出的字符
                       }

                       // 重置摩尔斯码序列和索引
                       memset(morse_sequence, 0, sizeof(morse_sequence));
                       morse_index = 0;
                   }

                   signal_end = 0;  // 重置信号结束时间
               }

               last_display_value = display_value;  // 更新上一个display_value状态
           }
       }
}


void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};


  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pin for the IR sensor */
  GPIO_InitStruct.Pin = IR_SENSOR_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT; // Set as input mode
  GPIO_InitStruct.Pull = GPIO_NOPULL; // No pull up resistor needed
  HAL_GPIO_Init(IR_SENSOR_GPIO_PORT, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
