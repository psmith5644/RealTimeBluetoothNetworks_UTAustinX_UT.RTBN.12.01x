#include "unity.h"
#include "os.h"
#include "os_Spy.h"
#include "FakeCortexM.h"
#include "FakeBSP.h"

void test_OS_Spy_GetSemaphoreValueOnInit(void) {
    int32_t * semaphorePtr = malloc(sizeof(int32_t));
    OS_InitSemaphore(semaphorePtr, 2);

    TEST_ASSERT_EQUAL_INT32(2, OS_Spy_GetSemaphoreValue(semaphorePtr));

    free(semaphorePtr);
}