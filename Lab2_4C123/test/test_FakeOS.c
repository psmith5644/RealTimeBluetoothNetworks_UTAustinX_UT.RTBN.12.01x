#include "unity.h"
#include "FakeOS.h"

void test_FakeSemaphoreInit(void) {
    int32_t * semaphorePtr;
    OS_InitSemaphore(semaphorePtr, 2);

    TEST_ASSERT_EQUAL_INT32(2, FakeOS_GetSemaphoreValue(semaphorePtr));
}