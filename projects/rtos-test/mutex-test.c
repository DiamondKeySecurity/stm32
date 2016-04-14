#include "cmsis_os.h"

#include "stm-init.h"
#include "stm-uart.h"

osMutexId stdio_mutex;
osMutexDef(stdio_mutex);

void notify(const char* name, int state) {
    osMutexWait(stdio_mutex, osWaitForever);
    //printf("%s: %d\n\r", name, state);
    uart_send_string(name);
    uart_send_string(": ");
    uart_send_integer(state, 1);
    uart_send_string("\r\n");
    osMutexRelease(stdio_mutex);
}

void test_thread(void const *args) {
    while (1) {
        notify((const char*)args, 0); osDelay(1000);
        notify((const char*)args, 1); osDelay(1000);
    }
}

void t2(void const *argument) {test_thread("Th 2");}
osThreadDef(t2, osPriorityNormal, DEFAULT_STACK_SIZE);

void t3(void const *argument) {test_thread("Th 3");}
osThreadDef(t3, osPriorityNormal, DEFAULT_STACK_SIZE);

int main() {
    stm_init();
    stdio_mutex = osMutexCreate(osMutex(stdio_mutex));

    osThreadCreate(osThread(t2), NULL);
    osThreadCreate(osThread(t3), NULL);

    test_thread((void *)"Th 1");
}
