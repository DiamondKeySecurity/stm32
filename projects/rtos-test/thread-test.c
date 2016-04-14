#include "cmsis_os.h"

#include "stm-init.h"
#include "stm-led.h"

void led2_thread(void const *args)
{
    while (1) {
        led_toggle(LED_BLUE);
        osDelay(1000);
    }
}
osThreadDef(led2_thread, osPriorityNormal, DEFAULT_STACK_SIZE);

int main()
{
    stm_init();
    osThreadCreate(osThread(led2_thread), NULL);
    
    while (1) {
        led_toggle(LED_GREEN);
        osDelay(500);
    }
}
