# Real-Time Bluetooth Networks Course

I will complete the UT Austin Real-Time Bluetooth Networks - Shape the World course.

# Learning Objectives

* Gain hands-on experience with RTOS by building many of the fundamental components of an RTOS
* Learn Bluetooth networking
* Complete a series of projects that will build upon my understanding of microcontroller peripheral interfacing
* Build a knowledge foundation with RTOS and Bluetooth that will aid in building my own future projects

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
