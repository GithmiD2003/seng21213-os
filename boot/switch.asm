[BITS 32]

global irq0_stub
extern scheduler_irq

; IRQ0 timer interrupt entry point
irq0_stub:
    pushad

    ; Pass the current stack pointer to the scheduler.
    mov eax, esp
    push eax
    call scheduler_irq
    add esp, 4

    ; Scheduler returns the stack pointer
    ; of the process we should run next.
    mov esp, eax

    popad

    ; Send End Of Interrupt (EOI) to the master PIC.
    mov al, 0x20
    out 0x20, al

    ; Return from the hardware interrupt.
    iretd