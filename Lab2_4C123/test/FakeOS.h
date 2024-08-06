#include "os.h"

void OS_InitSemaphore(int32_t * semaphorePtr, int32_t value);

int32_t FakeOS_GetSemaphoreValue(int32_t * semaphorePtr);