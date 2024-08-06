#include "unity.h"
#include "FakeOS.h"

void test_FakeSemaphoreInit(void) {
    int32_t * semaphorePtr = malloc(sizeof(int32_t));
    OS_InitSemaphore(semaphorePtr, 2);

    TEST_ASSERT_EQUAL_INT32(2, FakeOS_GetSemaphoreValue(semaphorePtr));

    free(semaphorePtr);
}