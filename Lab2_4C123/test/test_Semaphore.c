#include "unity.h"
#include "os.h"
#include "os_Spy.h"
#include "FakeCortexM.h"
#include "FakeBSP.h"

int32_t * semaphorePtr;

void setUp(void) {
    semaphorePtr = malloc(sizeof(int32_t));
    OS_InitSemaphore(semaphorePtr, 1);
}

void teardown(void) {
    free(semaphorePtr);
}

void test_SemaphoreInitValue(void) {
    int32_t value = OS_Spy_GetSemaphoreValue(semaphorePtr);

    TEST_ASSERT_EQUAL_INT32(1, value);
}

void test_SemaphoreWaitDecrement(void) {
    int32_t initialValue = OS_Spy_GetSemaphoreValue(semaphorePtr);
    OS_Wait(semaphorePtr);

    TEST_ASSERT_EQUAL_INT32(initialValue-1, OS_Spy_GetSemaphoreValue(semaphorePtr));

}

// #include "mock_os.h"