#include "inttypes.h"

#define KEYPRESS_1 1
#define KEYPRESS_2 (1 << 1)
#define KEYPRESS_3 (1 << 2)
#define KEYPRESS_A (1 << 3)
#define KEYPRESS_4 (1 << 4)
#define KEYPRESS_5 (1 << 5)
#define KEYPRESS_6 (1 << 6)
#define KEYPRESS_B (1 << 7)
#define KEYPRESS_7 (1 << 8)
#define KEYPRESS_8 (1 << 9)
#define KEYPRESS_9 (1 << 10)
#define KEYPRESS_C (1 << 11)
#define KEYPRESS_STAR (1 << 12)
#define KEYPRESS_0 (1 << 13)
#define KEYPRESS_DIEZ (1 << 14)
#define KEYPRESS_D (1 << 15)

#define TTW 150 / portTICK_PERIOD_MS
#define WAIT_A_BIT vTaskDelay(TTW)

void keyboard_init();

uint16_t scan_matrix();
char *get_key_from_code(uint16_t code);
void wait_for_key_release();
