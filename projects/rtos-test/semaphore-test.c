#include "cmsis_os.h"

#include "stm-init.h"
#include "stm-uart.h"

osSemaphoreId two_slots;
osSemaphoreDef(two_slots);

void test_thread(void const *name) {
    while (1) {
        osSemaphoreWait(two_slots, osWaitForever);
        //printf("%s\n\r", (const char*)name);
        uart_send_string((const char*)name);
        uart_send_string("\r\n");
        osDelay(1000);
        osSemaphoreRelease(two_slots);
    }
}

void t2(void const *argument) {test_thread("Th 2");}
osThreadDef(t2, osPriorityNormal, DEFAULT_STACK_SIZE);

void t3(void const *argument) {test_thread("Th 3");}
osThreadDef(t3, osPriorityNormal, DEFAULT_STACK_SIZE);

int main (void) {
    stm_init();
    two_slots = osSemaphoreCreate(osSemaphore(two_slots), 2);

    osThreadCreate(osThread(t2), NULL);
    osThreadCreate(osThread(t3), NULL);

    test_thread((void *)"Th 1");
}
