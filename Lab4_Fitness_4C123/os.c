// os.c
// Runs on LM4F120/TM4C123/MSP432
// A priority/blocking real-time operating system 
// Lab 4 starter file.
// Daniel Valvano
// March 25, 2016
// Hint: Copy solutions from Lab 3 into Lab 4
#include <stdint.h>
#include "os.h"
#include "CortexM.h"
#include "BSP.h"
#include "../inc/tm4c123gh6pm.h"

// function definitions in osasm.s
void StartOS(void);

#define NUMTHREADS  8        // maximum number of threads
#define NUMPERIODIC 2        // maximum number of periodic threads
#define STACKSIZE   100      // number of 32-bit words in stack per thread
struct tcb{
  int32_t *sp;       // pointer to stack (valid for threads not running
  struct tcb *next;  // linked-list pointer
  int32_t * blocked; // nonzero if blocked on this semaphore
  uint32_t sleepTime; // nonzero if this thread is sleeping
  uint32_t priority;
};
typedef struct tcb tcbType;
tcbType tcbs[NUMTHREADS];
tcbType *RunPt;
int32_t Stacks[NUMTHREADS][STACKSIZE];
void static runperiodicevents(void);

typedef struct {
  void (*thread)(void);
  uint32_t period;
  uint32_t timeUntilExecute;
} eventThread;

eventThread eventThreads[NUMPERIODIC];


void SetInitialStack(int i);

// ******** initializeThread ************
// Initializes a thread's stack and tcb data
// Input: the number of the thread to initialize
// Output: None
static void initializeThread(int threadNum) {
  SetInitialStack(threadNum);
  
  tcbs[threadNum].blocked = 0;
  tcbs[threadNum].sleepTime = 0;

  if (threadNum == NUMTHREADS-1) {
    tcbs[threadNum].next = &tcbs[0];
    return;
  }

  tcbs[threadNum].next = &tcbs[threadNum+1];
}

// ******** wakeupBlockedThread ************
// Searches threads for next thread blocked by semaphore
// Called with interrupts disabled
// Input: pointer to semaphore that some thread(s) are blocked on
// Output: None
static void wakeupBlockedThread(int32_t * semaPt) {
  tcbType * threadPtr = RunPt->next;

  while (threadPtr->blocked != semaPt) {
    threadPtr = threadPtr->next;
  }

  threadPtr->blocked = 0;
}

// ******** isThreadReady ************
// Tests whether a thread is ready to be run
// Input: thread to test
// Output: None
static int32_t isThreadReady(tcbType * threadPtr) {
  return (int32_t)(threadPtr->blocked == 0 && threadPtr->sleepTime == 0);
}

static void decrementSleepTimer(int32_t const i, int32_t const timeElapsed) {
  if (tcbs[i].sleepTime >= timeElapsed) {
    tcbs[i].sleepTime -= timeElapsed;
  } 
  else {
    tcbs[i].sleepTime = 0;
  }
}

#define UPDATE_THREAD_SLEEP_TIMERS_EXECUTIONS_PER_SEC 100
#define MS_PER_SECOND 1000
static void updateThreadSleepTimers(void) {
  DisableInterrupts();
  int32_t const timeElapsed = MS_PER_SECOND / UPDATE_THREAD_SLEEP_TIMERS_EXECUTIONS_PER_SEC;
  for (int i = 0; i < NUMTHREADS; i++) {
    decrementSleepTimer(i, timeElapsed);
  }
  EnableInterrupts();
}

static void decrementEventTimer(int32_t i, uint32_t timeElapsed) {
  if (eventThreads[i].timeUntilExecute >= timeElapsed) {
    eventThreads[i].timeUntilExecute -= timeElapsed;
  } 
  else {
    eventThreads[i].timeUntilExecute = 0;
  }
}

#define UPDATE_PERIODIC_EVENT_THREAD_TIMER_FREQ 1000
static void runPeriodicThreads(void) {
  int32_t const timeElapsed = MS_PER_SECOND / UPDATE_PERIODIC_EVENT_THREAD_TIMER_FREQ;
  DisableInterrupts();
  for (int i = 0; i < NUMPERIODIC; i++) {
    decrementEventTimer(i, timeElapsed);
    if (eventThreads[i].timeUntilExecute == 0) {
      eventThreads[i].thread();
      eventThreads[i].timeUntilExecute = eventThreads[i].period;
    }
  }
  EnableInterrupts();
}

// ******** OS_Init ************
// Initialize operating system, disable interrupts
// Initialize OS controlled I/O: periodic interrupt, bus clock as fast as possible
// Initialize OS global variables
// Inputs:  none
// Outputs: none
void OS_Init(void){
  DisableInterrupts();
  BSP_Clock_InitFastest();// set processor clock to fastest speed
// perform any initializations needed, 
// set up periodic timer to run runperiodicevents to implement sleeping
  BSP_PeriodicTask_Init(&updateThreadSleepTimers, UPDATE_THREAD_SLEEP_TIMERS_EXECUTIONS_PER_SEC, 2);
  // BSP_PeriodicTask_InitB(&runPeriodicThreads, UPDATE_PERIODIC_EVENT_THREAD_TIMER_FREQ, 2);
}


void SetInitialStack(int i){
  tcbs[i].sp = &Stacks[i][STACKSIZE-16]; // thread stack pointer
  Stacks[i][STACKSIZE-1] = 0x01000000; // Thumb bit
  Stacks[i][STACKSIZE-3] = 0x14141414; // R14
  Stacks[i][STACKSIZE-4] = 0x12121212; // R12
  Stacks[i][STACKSIZE-5] = 0x03030303; // R3
  Stacks[i][STACKSIZE-6] = 0x02020202; // R2
  Stacks[i][STACKSIZE-7] = 0x01010101; // R1
  Stacks[i][STACKSIZE-8] = 0x00000000; // R0
  Stacks[i][STACKSIZE-9] = 0x11111111; // R11
  Stacks[i][STACKSIZE-10] = 0x10101010; // R10
  Stacks[i][STACKSIZE-11] = 0x09090909; // R9
  Stacks[i][STACKSIZE-12] = 0x08080808; // R8
  Stacks[i][STACKSIZE-13] = 0x07070707; // R7
  Stacks[i][STACKSIZE-14] = 0x06060606; // R6
  Stacks[i][STACKSIZE-15] = 0x05050505; // R5
  Stacks[i][STACKSIZE-16] = 0x04040404; // R4
 
}

//******** OS_AddThreads ***************
// Add eight main threads to the scheduler
// Inputs: function pointers to eight void/void main threads
//         priorites for each main thread (0 highest)
// Outputs: 1 if successful, 0 if this thread can not be added
// This function will only be called once, after OS_Init and before OS_Launch
int OS_AddThreads(void(*thread0)(void), uint32_t p0,
                  void(*thread1)(void), uint32_t p1,
                  void(*thread2)(void), uint32_t p2,
                  void(*thread3)(void), uint32_t p3,
                  void(*thread4)(void), uint32_t p4,
                  void(*thread5)(void), uint32_t p5,
                  void(*thread6)(void), uint32_t p6,
                  void(*thread7)(void), uint32_t p7){
  for (int i = 0; i < NUMTHREADS; i++) {
    initializeThread(i);
  }

  Stacks[0][STACKSIZE-2] = (int32_t)(thread0); // PC
  Stacks[1][STACKSIZE-2] = (int32_t)(thread1); // PC
  Stacks[2][STACKSIZE-2] = (int32_t)(thread2); // PC
  Stacks[3][STACKSIZE-2] = (int32_t)(thread3); // PC
  Stacks[4][STACKSIZE-2] = (int32_t)(thread4); // PC
  Stacks[5][STACKSIZE-2] = (int32_t)(thread5); // PC
  Stacks[6][STACKSIZE-2] = (int32_t)(thread6); // PC
  Stacks[7][STACKSIZE-2] = (int32_t)(thread7); // PC

  tcbs[0].priority = p0;
  tcbs[1].priority = p1;
  tcbs[2].priority = p2;
  tcbs[3].priority = p3;
  tcbs[4].priority = p4;
  tcbs[5].priority = p5;
  tcbs[6].priority = p6;
  tcbs[7].priority = p7;

  RunPt = &tcbs[0];

  return 1;               // successful 
}


void static runperiodicevents(void){
// ****IMPLEMENT THIS****
// **DECREMENT SLEEP COUNTERS
// In Lab 4, handle periodic events in RealTimeEvents
  
}

//******** OS_Launch ***************
// Start the scheduler, enable interrupts
// Inputs: number of clock cycles for each time slice
// Outputs: none (does not return)
// Errors: theTimeSlice must be less than 16,777,216
void OS_Launch(uint32_t theTimeSlice){
  STCTRL = 0;                  // disable SysTick during setup
  STCURRENT = 0;               // any write to current clears it
  SYSPRI3 =(SYSPRI3&0x00FFFFFF)|0xE0000000; // priority 7
  STRELOAD = theTimeSlice - 1; // reload value
  STCTRL = 0x00000007;         // enable, core clock and interrupt arm
  StartOS();                   // start on the first task
}

#define LOWEST_PRIORITY 255
// runs every ms
void Scheduler(void){      // every time slice
  // ****IMPLEMENT THIS****
  // look at all threads in TCB list choose
  // highest priority thread not blocked and not sleeping 
  // If there are multiple highest priority (not blocked, not sleeping) run these round robin
  tcbType * threadPt = RunPt;
  tcbType * highestPriorityThread;
  uint32_t highestPriorityLevel = LOWEST_PRIORITY;

  do {
    threadPt = threadPt->next;
    if (isThreadReady(threadPt) && threadPt->priority < highestPriorityLevel) {
      highestPriorityThread = threadPt;
      highestPriorityLevel = highestPriorityThread->priority;
    }
  } while (threadPt != RunPt);

  RunPt = highestPriorityThread;
}

//******** OS_Suspend ***************
// Called by main thread to cooperatively suspend operation
// Inputs: none
// Outputs: none
// Will be run again depending on sleep/block status
void OS_Suspend(void){
  STCURRENT = 0;        // any write to current clears it
  INTCTRL = 0x04000000; // trigger SysTick
// next thread gets a full time slice
}

// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(uint32_t sleepTime){
// ****IMPLEMENT THIS****
// set sleep parameter in TCB, same as Lab 3
// suspend, stops running
  DisableInterrupts();
  RunPt->sleepTime = sleepTime;
  EnableInterrupts();
  OS_Suspend();

}

// ******** OS_InitSemaphore ************
// Initialize counting semaphore
// Inputs:  pointer to a semaphore
//          initial value of semaphore
// Outputs: none
void OS_InitSemaphore(int32_t *semaPt, int32_t value){
// ****IMPLEMENT THIS****
  *semaPt = value;
 
}

// ******** OS_Wait ************
// Decrement semaphore and block if less than zero
// Lab2 spinlock (does not suspend while spinning)
// Lab3 block if less than zero
// Inputs:  pointer to a counting semaphore
// Outputs: none
void OS_Wait(int32_t *semaPt){
  DisableInterrupts();
  (*semaPt)--;

  if (*semaPt < 0) {
    RunPt->blocked = semaPt;
    OS_Suspend();
  }

  EnableInterrupts();
}

// ******** OS_Signal ************
// Increment semaphore
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate
// Inputs:  pointer to a counting semaphore
// Outputs: none
void OS_Signal(int32_t *semaPt){
  DisableInterrupts();
  (*semaPt)++;

  if (*semaPt <= 0) {
    wakeupBlockedThread(semaPt);
  } 

  EnableInterrupts();
}

#define FSIZE 10    // can be any size
uint32_t PutI;      // index of where to put next
uint32_t GetI;      // index of where to get next
uint32_t Fifo[FSIZE];
int32_t CurrentSize;// 0 means FIFO empty, FSIZE means full
uint32_t LostData;  // number of lost pieces of data

// ******** incrementFifoIndex ************
// Increments and wraps a Fifo indexer, i.e. PutI and GetI
// Input: the index to increment
// Output: None
static void incrementFifoIndex(uint32_t * index) {
  if (*index == FSIZE-1) {
    *index = 0;
  } 
  else {
    (*index)++;
  }
}

// ******** OS_FIFO_Init ************
// Initialize FIFO.  
// One event thread producer, one main thread consumer
// Inputs:  none
// Outputs: none
void OS_FIFO_Init(void){
  PutI = Fifo[0];
  GetI = Fifo[0];
  LostData = 0;

  OS_InitSemaphore(&CurrentSize, 0);
}


// ******** OS_FIFO_Put ************
// Put an entry in the FIFO.  
// Exactly one event thread puts,
// do not block or spin if full
// Inputs:  data to be stored
// Outputs: 0 if successful, -1 if the FIFO is full
int OS_FIFO_Put(uint32_t data){
  if (CurrentSize == FSIZE) {
    LostData++;
    return -1; // full
  }

  Fifo[PutI] = data;
  incrementFifoIndex(&PutI);
  OS_Signal(&CurrentSize);

  return 0;   // success
}

// ******** OS_FIFO_Get ************
// Get an entry from the FIFO.   
// Exactly one main thread get,
// do block if empty
// Inputs:  none
// Outputs: data retrieved
uint32_t OS_FIFO_Get(void){
  uint32_t data;

  OS_Wait(&CurrentSize);
  data = Fifo[GetI];
  incrementFifoIndex(&GetI);

  return data;
}

// *****periodic events****************
int32_t *PeriodicSemaphore0;
uint32_t Period0; // time between signals
int32_t *PeriodicSemaphore1;
uint32_t Period1; // time between signals
void RealTimeEvents(void){int flag=0;
  static int32_t realCount = -10; // let all the threads execute once
  // Note to students: we had to let the system run for a time so all user threads ran at least one
  // before signalling the periodic tasks
  realCount++;
  if(realCount >= 0){
		if((realCount%Period0)==0){
      OS_Signal(PeriodicSemaphore0);
      flag = 1;
		}
    if((realCount%Period1)==0){
      OS_Signal(PeriodicSemaphore1);
      flag=1;
		}
    if(flag){
      OS_Suspend();
    }
  }
}
// ******** OS_PeriodTrigger0_Init ************
// Initialize periodic timer interrupt to signal 
// Inputs:  semaphore to signal
//          period in ms
// priority level at 0 (highest
// Outputs: none
void OS_PeriodTrigger0_Init(int32_t *semaPt, uint32_t period){
	PeriodicSemaphore0 = semaPt;
	Period0 = period;
	BSP_PeriodicTask_InitC(&RealTimeEvents,1000,0);
}
// ******** OS_PeriodTrigger1_Init ************
// Initialize periodic timer interrupt to signal 
// Inputs:  semaphore to signal
//          period in ms
// priority level at 0 (highest
// Outputs: none
void OS_PeriodTrigger1_Init(int32_t *semaPt, uint32_t period){
	PeriodicSemaphore1 = semaPt;
	Period1 = period;
	BSP_PeriodicTask_InitC(&RealTimeEvents,1000,0);
}

//****edge-triggered event************
int32_t *edgeSemaphore;
// ******** OS_EdgeTrigger_Init ************
// Initialize button1, PD6, to signal on a falling edge interrupt
// Inputs:  semaphore to signal
//          priority
// Outputs: none
void OS_EdgeTrigger_Init(int32_t *semaPt, uint8_t priority) {
	edgeSemaphore = semaPt;
  SYSCTL_RCGCGPIO_R |= SYSCTL_RCGC2_GPIOD; // activate clock for Port D
  while((SYSCTL_PRGPIO_R & SYSCTL_PRGPIO_R3) == 0) {} // allow time for clock to stabilize
  GPIO_PORTD_AMSEL_R &= ~(1 << 6); // disable analog on PD6
  GPIO_PORTD_DIR_R &= ~(1 << 6);   // make PD6 input
  GPIO_PORTD_AFSEL_R &= ~(1 << 6); // disable alt funct on PD6
  GPIO_PORTD_PUR_R &= ~(1 << 6);   // disable pull-up on PD6
  GPIO_PORTD_DEN_R |= (1 << 6);    // enable digital I/O on PD6  
  GPIO_PORTD_IS_R &= ~(1 << 6);    // PD6 is edge-sensitive 
  GPIO_PORTD_IBE_R &= ~(1 << 6);   // PD6 is not both edges 
  GPIO_PORTD_IEV_R &= ~(1 << 6);   // PD6 is falling edge event 
  GPIO_PORTD_ICR_R |= (1 << 6);    // clear PD6 flag
  GPIO_PORTD_IM_R |= (1 << 6);     // arm interrupt on PD6
  NVIC_PRI0_R = (NVIC_PRI0_R & ~NVIC_PRI0_INT3_M) | (priority << NVIC_PRI0_INT3_S); // set interrupt priority
  NVIC_EN0_R |= (1 << 3); // enable is bit 3 in NVIC_EN0_R
 }

// ******** OS_EdgeTrigger_Restart ************
// restart button1 to signal on a falling edge interrupt
// rearm interrupt
// Inputs:  none
// Outputs: none
void OS_EdgeTrigger_Restart(void){
//***IMPLEMENT THIS***
// rearm interrupt 3 in NVIC
// clear flag6
}
void GPIOPortD_Handler(void){
//***IMPLEMENT THIS***
	// step 1 acknowledge by clearing flag
  // step 2 signal semaphore (no need to run scheduler)
  // step 3 disarm interrupt to prevent bouncing to create multiple signals
}


