
#include "main.h"

#include <stdio.h>  // 包含 sprintf 函数的声明
#include <string.h> // 包含 strlen 函数的声明

ADC_HandleTypeDef hadc1;
UART_HandleTypeDef huart2;

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_GPIO_Init(void);
uint16_t Read_ADC(void);
uint8_t Read_DO(void);

#define MAX_MORSE_LENGTH 6  // 摩尔斯电码单个字符的最大长度
#define CHAR_GAP_MS 1000    // 字符间隔时间（毫秒）
#define DOT_LENGTH 50     // 点的长度（毫秒）
#define DASH_LENGTH 150     // 划的长度（毫秒）

volatile uint32_t signal_start_time = 0;
volatile uint32_t signal_end_time = 0;
volatile uint8_t signal_detected = 0;
char morseCodeString[MAX_MORSE_LENGTH + 1] = {0};  // 存储摩尔斯电码的字符串
int morseCodeIndex = 0;  // 摩尔斯电码的当前索引位置
uint32_t last_signal_end_time = 0;  // 上一个信号的结束时间

const char* MORSE_CODE_TABLE[36] = {
    ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",
    "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",
    "..-", "...-", ".--", "-..-", "-.--", "--..", "-----", ".----", "..---",
    "...--", "....-", ".....", "-....", "--...", "---..", "----."
};

// 查找摩尔斯电码对应的字母或数字
char MorseCodeToChar(const char* morseCode) {
    for (int i = 0; i < 36; i++) {
        if (strcmp(morseCode, MORSE_CODE_TABLE[i]) == 0) {
            return i < 26 ? 'A' + i : '0' + (i - 26);
        }
    }
    return '\0';
}

uint8_t Detect_Morse_Code(void) {
    uint32_t duration = 0;
    uint8_t code = 0;    // 0表示无信号，1表示点，2表示划

    // 等待PA0变为高电平
    while(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET);

    // 开始计时
    uint32_t start_time = HAL_GetTick();

    // 等待PA0变回低电平
    while(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET);

    // 计算持续时间
    duration = HAL_GetTick() - start_time;

    // 根据持续时间判断是点还是划
    if(duration >= DOT_LENGTH && duration < DASH_LENGTH) {
        code = 1; // 点
    } else if(duration >= DASH_LENGTH) {
        code = 2; // 划
    }

    return code;
}

/* Main function */
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_USART2_UART_Init();

    while (1) {
        uint32_t current_time = HAL_GetTick();
        uint8_t morseCodeSignal = Detect_Morse_Code();

        if (morseCodeSignal) {
            morseCodeString[morseCodeIndex++] = morseCodeSignal == 1 ? '.' : '-';
            last_signal_end_time = current_time;
        }

        if ((current_time - last_signal_end_time >= CHAR_GAP_MS && morseCodeIndex > 0) || morseCodeIndex == MAX_MORSE_LENGTH) {
            morseCodeString[morseCodeIndex] = '\0';  // 结束字符串
            char letter = MorseCodeToChar(morseCodeString);
            if (letter != '\0') {
                char msg[2] = {letter, '\0'};
                HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
            }
            morseCodeIndex = 0;
            memset(morseCodeString, 0, sizeof(morseCodeString));
        }
    }
}

uint8_t Read_DO(void) {
    return (uint8_t)HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
}

uint16_t Read_ADC(void) {
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY) == HAL_OK) {
        return HAL_ADC_GetValue(&hadc1);
    }
    return 0; // 或者您可以选择一个合适的错误处理方式
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
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


static void MX_ADC1_Init(void)
{



  ADC_ChannelConfTypeDef sConfig = {0};


  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
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

  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }


}


static void MX_USART2_UART_Init(void)
{

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

}


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

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

void Error_Handler(void)
{
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


//#include "main.h"
//#include <stdio.h>
//#include <string.h>
//
//UART_HandleTypeDef huart2;
//
//void SystemClock_Config(void);
//static void MX_USART2_UART_Init(void);
//static void MX_GPIO_Init(void);
//
///* Main function */
//int main(void) {
//    /* MCU Configuration */
//    HAL_Init();
//    SystemClock_Config();
//
//    /* Initialize all configured peripherals */
//    MX_GPIO_Init();
//    MX_USART2_UART_Init();
//
//    /* Variables for GPIO state tracking */
//    uint8_t last_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
//
//    /* Infinite loop */
//    while (1) {
//        uint8_t current_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0); // Read the current state of GPIOA PIN 0
//
//        // Check for state change
//        if (current_state != last_state) {
//            last_state = current_state; // Update the last_state
//
//            // Send the result
//            char msg[30];
//            if (current_state == GPIO_PIN_SET) {
//                sprintf(msg, "Signal HIGH\r\n");
//            } else {
//                sprintf(msg, "Signal LOW\r\n");
//            }
//            HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
//        }
//
//        HAL_Delay(100); // Add a short delay to reduce CPU usage
//    }
//}
//
///* USART2 init function */
//static void MX_USART2_UART_Init(void) {
//    huart2.Instance = USART2;
//    huart2.Init.BaudRate = 115200;
//    huart2.Init.WordLength = UART_WORDLENGTH_8B;
//    huart2.Init.StopBits = UART_STOPBITS_1;
//    huart2.Init.Parity = UART_PARITY_NONE;
//    huart2.Init.Mode = UART_MODE_TX_RX;
//    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
//    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
//    if (HAL_UART_Init(&huart2) != HAL_OK) {
//        Error_Handler();
//    }
//}
//
///* GPIO init function */
//static void MX_GPIO_Init(void) {
//    GPIO_InitTypeDef GPIO_InitStruct = {0};
//
//    /* GPIO Ports Clock Enable */
//    __HAL_RCC_GPIOA_CLK_ENABLE();
//
//    /*Configure GPIO pin : PA0 */
//    GPIO_InitStruct.Pin = GPIO_PIN_0;
//    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
//    GPIO_InitStruct.Pull = GPIO_NOPULL;
//    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
//}
//
//void Error_Handler(void) {
//    /* USER CODE BEGIN Error_Handler */
//    /* User can add his own implementation to report the file name and line number,
//       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
//    /* USER CODE END Error_Handler */
//    while (1) {
//    }
//}
//
//void SystemClock_Config(void) {
//    // System Clock Configuration
//    // This function is configured based on the specific requirements of the application
//    // You should adjust this function to suit your system's clock configuration
//}
//
//#ifdef USE_FULL_ASSERT
//void assert_failed(uint8_t *file, uint32_t line) {
//    /* USER CODE BEGIN 6 */
//    /* User can add his own implementation to report the file name and line number,
//       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
//    /* USER CODE END 6 */
//}
//#endif /* USE_FULL_ASSERT */
