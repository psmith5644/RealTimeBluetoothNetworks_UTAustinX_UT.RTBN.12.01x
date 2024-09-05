;/*****************************************************************************/
; OSasm.s: low-level OS commands, written in assembly                       */
; Runs on LM4F120/TM4C123/MSP432
; Lab 4 starter file
; March 25, 2016

;


        AREA |.text|, CODE, READONLY, ALIGN=2
        THUMB
        REQUIRE8
        PRESERVE8

        EXTERN  RunPt            ; currently running thread
        EXPORT  StartOS
        EXPORT  SysTick_Handler
        IMPORT  Scheduler


SysTick_Handler                ; 1) Saves R0-R3,R12,LR,PC,PSR
    CPSID   I                  ; 2) Prevent interrupt during switch

    PUSH    {R4-R11}           ; 3) Push R4-R11
    LDR     R0, =RunPt         ; 4) Load pointer to RunPt
    LDR     R1, [R0]           ; 4.5) Load current RunPt into R1
    STR     SP, [R1]           ; 5) Store Real SP into TCB SP
    PUSH    {R0, LR}           ; 6) Call Scheduler
    BL      Scheduler
    POP     {R0, LR}
    LDR     R1, [R0]           ; 6.5) Scheduler returns new RunPt, store into R1 
    LDR     SP, [R1]           ; 7) SP = RunPt->SP
    POP     {R4-R11}           ; 8) Pop R4-R11

    CPSIE   I                  ; 9) tasks run with interrupts enabled
    BX      LR                 ; 10) restore R0-R3,R12,LR,PC,PSR
    
StartOS    
    LDR     R0, =RunPt       ; Load address of RunPt into R0   
    LDR     R1, [R0]         ; R1 = value of RunPt (pointer to current thread)   
    LDR     SP, [R1]         ; Load new thread SP (SP = RunPt->sp)   
    POP     {R4-R11}         ; Restore registers R4-R11   
    POP     {R0-R3}          ; Restore registers R0-R3   
    POP     {R12}            ; Restore register R12
    ADD     SP, SP, #4       ; Discard LR from initial stack   
    POP     {LR}             ; Load return address into LR   
    ADD     SP, SP, #4       ; Discard PSR   

    CPSIE   I                  ; Enable interrupts at processor level
    BX      LR                 ; start first thread

    ALIGN
    END
