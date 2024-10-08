# Real-Time Bluetooth Networks Course

I will complete the UT Austin Real-Time Bluetooth Networks - Shape the World course.

# Learning Objectives

* Gain hands-on experience with RTOS by building many of the fundamental components of an RTOS
* Learn Bluetooth networking
* Complete a series of projects that will build upon my understanding of microcontroller peripheral interfacing
* Build a knowledge foundation with RTOS and Bluetooth that will aid in building my own future projects


# Lab 2

In Lab 2, I implemented semaphores, simple mailboxes that rely on semaphores, multithreading, and bootstrapping the operating system.  

The semaphores use OS_SemaphoreInit, OS_Wait, and OS_Signal to allow for controlled access to shared data.  OS_Wait will cause a thread to wait for the semaphore value to be greater than 0; then, it will decrement the semaphore and continue.  OS_Signal increments the semaphore, allowing waiting threads to gain access to the resource.  Waiting is currently implemented as a spinlock, which will be improved upon in future labs.

The simple mailbox uses a semaphore to control access to the data sent through the mailbox.  OS_Mailbox_Send is designed to be called from an event thread, and so does not spinlock or block if the mailbox is full.  Rather, it simply overwrites the data in the mailbox (a single integer, later to be implemented as a FIFO queue) and signals the semaphore to signify that there is data in the mailbox to be processed.  OS_Mailbox_Recv is designed to be called from a main thread, and so uses OS_Wait to wait until the semaphore indicates that there is data to be processed, and returns that data.

The multithreading was implemented by giving each thread its own stack and a management data structure called a thread control block that stores each thread's stack pointer and a pointer to the next TCB.  When the SysTick ISR occurs, the current thread has its context (registers) stored on its own stack, with the current SP saved into that thread's TCB.  Then the scheduler picks a new thread (round-robin in this case) and loads (pops) that context (registers) from that thread's stack into the actual CPU registers, including the stack pointer from that thread's TCB.  The OS_Init function prepares for this functionality by initializing each thread's TCB and stack such that they are ready to be loaded whenever that thread gets switched to.  The OS_Launch function initializes SysTick and calls the StartOS function, which begins the threads by loading the first thread context into the CPU; it enables interrupts, allowing SysTick to handle the context switching from here.  This version of the OS runs short periodic threads in the scheduler after preempting the current main thread and before starting the next main thread; this occurs at a period determined by the event thread at an interval that is a multiple of the main thread timeslice.  The main threads are preempted at a regular timeslice, in this case 1 millisecond.

# Lab 3

### Blocking Semaphores
In Lab 3, I refactored the spinlock semaphores to be blocking semaphores.  This increases the overall efficiency of the RTOS by not wasting processor time spinning.
The blocking semaphores allow for thread cooperation, where a thread can voluntarily give up its access to the processor while waiting on a semaphore.
To implement this, a 'blocked' field is added to the thread control block.  This field is 0 when the thread is unblocked; when blocked, it contains a pointer to the semaphore it is blocked on.
The scheduler ignores threads that are blocked.  
Another thread may signal the semaphore, which will then look for the next thread (in round-robin order) that is blocked on that semaphore and unblock it, allowing it to be scheduled again.
This is not the most efficient implementation of blocking.  
A superior implementation would remove a blocked thread from the linked list of ready-to-run threads and add it to a linked list of threads blocked on that specific semaphore.  
This would ensure that whenever a context switch occurs, the scheduler does not waste time by having to check blocked threads to see if they are ready-to-run.  
It would also allow the RTOS to track the order in which threads on a particular semaphore blocked, and unblock the one that has been blocked longest.

### FIFO Queue
A global FIFO queue was implemented. It consists of the functions Init, Get, and Put.  Put is designed to be called from a non-blocking event thread.
Get is designed to be called from a blocking main thread.  This means that Put will not wait on a semaphore that indicates the space left in the FIFO.
As a result, data may be lost.  In this case, lost data is accepted; 
in other scenarios, this may be unacceptable, and the implementation would need to be changed such that Put is intended to be called from a blocking main thread.

### Thread Sleeping
I also implemented the ability for threads to sleep.  This functionality allows a thread to voluntarily give up the processor to wait for a specific amount of time. 
This is used, for example, by sensors that have a known sample rate; a thread can initiate a sample, sleep until the sample is completed, and then do some computation with the data.
Sleeping is implemented by adding a sleepTime variable to the thread control block.  A periodic timer is used to decrease the sleepTime variable of all sleeping threads at a regular interval.
When a thread's sleepTime is nonzero, it is ignored by the scheduler.  When it is 0, it is not sleeping and is ready to run.

### Periodic Event Threads
Periodic Timers were used to track the sleep timers of sleeping threads and to schedule periodic event threads.  
These event threads occur at regular intervals, have very short runtimes, and do not block.
An Event Thread struct is used to track the period of each periodic thread and the time remaining until its next desired execution.
A periodic timer decreases the time remaining until execution for each event thread and runs the thread at the appropriate time.

# Lab 4

### Priority Scheduler
Implemented a simple priority scheduler that always runs the highest priority thread among those available. When multiple threads equal the highest priority, the scheduler runs them in round-robin order. 

### Replace Periodic Event Threads
Periodic event threads have had their logic moved to main threads.  They wait on semaphores that are signalled at the appropriate period.  This implementation accounts for priority, which was not possible with the previous implementation that simply ran the event threads periodically. A periodic timer that decrements thread sleep countdowns remains.  

### Button Debouncing with Interrupts
An edge-triggered interrupt was enabled for Button 1 of the TI Educational BoosterPack MKII. The ISR simply signals a semaphore, allowing a main thread to proceed with the response to the button press. 
Button debouncing is handled by masking the interrupt temporarily for a short period after an interrupt from the button press occurs, and only then reading the state of the button signal.

# Lab 5

### Simple Write-once File System
Implemented a simple write-once file system.  The system allows for files to be created, appended to and read, though not to be deleted or edited. 
Files can be appended to and read in 512 byte blocks, or sectors.  These sectors are tracked by a directory and File Allocation Table (FAT). 
The nth entry of the directory contains the number of the first sector of the nth file.
The mth entry of the FAT contains the sector following sector m in the file.
This implementation uses half of the Flash EEPROM as the disk.  This storage allows for 255 sectors of 512 bytes each.
The 255th sector is reserved for the Directory and FAT.  As such, the value 255 is used in the directory and FAT to indicate there are no more sectors in the file.
This simple file system has limited application, but could be used for any sort of logging.

# Lab 6

### BLE and Simple Network Processor API Implementation

In Lab 6, I implemented several methods that construct messages to be sent to the CC2650 Network Processor using the Simple Network Process API.  This interface allows the Application Processor, in this case a TivaC Launchpad, to configure Bluetooth Low Energy services that are implemented by the CC2650.   The implemented messages allow for BLE services and attributes to be configured, and allow for the NP to begin advertising its data.  

Some issues were encountered: When connecting to the NP via my smartphone, only some of the attributes appear, despite the messages saying that all of the attributes were successfully registered.  However, all of the messages I implemented are constructed correctly according to the provided grader.  I manually decoded several messages and message responses and determined that all of the messages appear to be sent correctly and receive responses that indicate success, so the issue does not appear to be with my implementation of the SNP message builders, which is the vast majority the implementation of this lab.  If I remove the first few characteristics from being added to the service, the previously unseen characteristics appear.  I'm not sure what sort of issue would cause this behavior, given that the messages are being constructed and sent correctly.

Because the application seems to function correctly at the SNP API level, I examined the provided functions such as Lab6_AddCharacteristic() to see if the issue lied there.  But this function seems to be running correctly in each instantiation, always returning a success code, and each time it sends a message it receives success codes.  I thought then that the issue may lie in the RTOS, which is my implementation from lab3 that was pasted into this project, but the rest of the system runs fine; additionally, the alternative Lab6 does not use an RTOS and also encounters the same issue, so the issue does not appear to be caused from my RTOS.  This leaves the LightBlue app, that I use to connect to the NP, as a potential culprit.  Although it is of course possible that there is some issue with my SNP messages, the provided Lab6 application, or my RTOS that I have been unable to discover. 

I have decided to move on from the project because as far as I can tell, I implemented all of the SNP messages correctly, which is the focus of the project, and some correct functionality has been delivered.  Rather than continue to debug this, I will continue learning other topics in embedded systems.
