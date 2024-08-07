#include "unity.h"
#include "os.h"
#include "os_Spy.h"
#include "FakeCortexM.h"
#include "FakeBSP.h"


void test_SemaphoreInitValue(void) {
    int32_t * semaphorePtr = malloc(sizeof(int32_t));
    OS_InitSemaphore(semaphorePtr, 1);
    int32_t value = OS_Spy_GetSemaphoreValue(semaphorePtr);

    TEST_ASSERT_EQUAL_INT32(1, value);

    free(semaphorePtr);
}

void test_SemaphoreWaitDecrement(void) {
    int32_t * semaphorePtr = malloc(sizeof(int32_t));
    OS_InitSemaphore(semaphorePtr, 1);
    
    int32_t initialValue = OS_Spy_GetSemaphoreValue(semaphorePtr);
    OS_Wait(semaphorePtr);

    TEST_ASSERT_EQUAL_INT32(initialValue+1, OS_Spy_GetSemaphoreValue(semaphorePtr));

    free(semaphorePtr);
}

// #include "mock_os.h"