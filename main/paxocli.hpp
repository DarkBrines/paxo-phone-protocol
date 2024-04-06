#include "driver/uart.h"

#define UART_PORT UART_NUM_0
#define MAX_CMD_COMPONENTS 4
#define MAX_CMD_COMPONENT_SIZE 512

void paxocli_init();