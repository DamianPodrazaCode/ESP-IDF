#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"

#define BLINK_GPIO 12

void app_main(void)
{
    ESP_LOGI("111","222");  // Macro to output logs at ESP_LOG_INFO level.
    printf("Start app_main !!!!\n"); // Standardowe wyjście uart0, czyli to którym programujemy

    // prosty sposób
    // gpio_reset_pin(BLINK_GPIO);                       
    // gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    // pełny sposób
	gpio_config_t io_conf;
	io_conf.pin_bit_mask = (1ULL << BLINK_GPIO); 
	io_conf.mode = GPIO_MODE_OUTPUT;              
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;    
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	io_conf.intr_type = GPIO_INTR_DISABLE;       
	gpio_config(&io_conf);     
    
    // siła wysterowania, prędkość zbocza
    // GPIO_DRIVE_CAP_0  //ok. 5 mA - narastanie 100ns
	// GPIO_DRIVE_CAP_1  //ok. 10 mA (często domyślny)
	// GPIO_DRIVE_CAP_2  //(lub GPIO_DRIVE_CAP_DEFAULT): ok. 20 mA
	// GPIO_DRIVE_CAP_3  //ok. 40 mA - 40ns    
    esp_err_t err = gpio_set_drive_capability(BLINK_GPIO, GPIO_DRIVE_CAP_3);
    if (err == ESP_OK)
    {
    }

    while (1)
    {
        // gpio_set_level(BLINK_GPIO, 1); // przełączenie 240ns, nie ważne jaka moc narastania
        // gpio_set_level(BLINK_GPIO, 0);
        // gpio_set_level(BLINK_GPIO, 1);
        // gpio_set_level(BLINK_GPIO, 0);
        // gpio_set_level(BLINK_GPIO, 1);
        // gpio_set_level(BLINK_GPIO, 0);
        // gpio_set_level(BLINK_GPIO, 1);
        // gpio_set_level(BLINK_GPIO, 0);
        // vTaskDelay(10 / portTICK_PERIOD_MS); // Czekaj żeby nie zawiesić rtos

        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(100 / portTICK_PERIOD_MS); // Czekaj żeby nie zawiesić rtos
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(100 / portTICK_PERIOD_MS); // Czekaj żeby nie zawiesić rtos

    }
}