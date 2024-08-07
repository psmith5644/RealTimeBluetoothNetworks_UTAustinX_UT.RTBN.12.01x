#include "unity.h"
#include "mock_FakeOS.h"
#include "FakeOS.h"
#include <stdlib.h>

void test_SemaphoreSpinlockWhenWaitAtZero(void) {
    int32_t * semaPt = malloc(sizeof(int32_t));

    mock_FakeOS_Init();
    OS_InitSemaphore_Expect(semaPt, 0);
    OS_Wait_Expect(semaPt);
    OS_Spinlock_Expect(semaPt);

    OS_InitSemaphore(semaPt, 0);
    OS_Wait(semaPt);

    free(semaPt);
}
