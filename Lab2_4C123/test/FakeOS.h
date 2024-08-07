#include "os.h"

void OS_Wait(int32_t * semaPt);
void OS_Spinlock(int32_t * semaPt);
void OS_InitSemaphore(int32_t * semaPt, int32_t value);