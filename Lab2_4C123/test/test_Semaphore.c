#include "unity.h"
#include "FakeOS.h"

void test_SemaphoreInitValue(void) {
    int32_t * semaphorePtr;
    OS_InitSemaphore(semaphorePtr, 1);
    int32_t value = FakeOS_GetSemaphoreValue(semaphorePtr);

    TEST_ASSERT_EQUAL_INT32(1, value);
}