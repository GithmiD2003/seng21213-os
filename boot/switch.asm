[BITS 32]

global irq0_stub
global thread_start_trampoline

extern scheduler_irq
extern thread_start

thread_start_trampoline:
    push eax
    call thread_start
    add esp, 4

.hang:
    cli
    hlt
    jmp .hang

irq0_stub:
    pushad

    mov eax, esp
    push eax
    call scheduler_irq
    add esp, 4

    mov ebx, eax

    mov al, 0x20
    out 0x20, al

    mov esp, ebx

    popad
    iretd