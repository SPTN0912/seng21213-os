[BITS 32]

; =============================================================================
; SENG21213 OS - Stage 1
; Context switching and IRQ0 timer handler
; =============================================================================

GLOBAL switch_context
GLOBAL irq0_handler

EXTERN scheduler_tick_context

; -----------------------------------------------------------------------------
; switch_context
;
; void switch_context(uint32_t *old_esp, uint32_t new_esp);
;
; Saves the current register state and switches to another saved stack.
; -----------------------------------------------------------------------------

switch_context:
    pushad

    ; Save the current stack pointer.
    mov eax, [esp + 36]
    mov [eax], esp

    ; Load the next process's saved stack pointer.
    mov esp, [esp + 40]

    ; Restore the next process's registers.
    popad

    ret


; -----------------------------------------------------------------------------
; irq0_handler
;
; Called by the PIT timer approximately every 10 ms.
;
; The CPU has already pushed:
;   EFLAGS
;   CS
;   EIP
;
; We then save the general-purpose registers with PUSHAD.
;
; scheduler_tick_context() receives the current saved ESP and
; returns the ESP of the process that should run next.
; -----------------------------------------------------------------------------

irq0_handler:
    ; Save all general-purpose registers.
    pushad

    ; Pass the address of the saved-register area to C.
    push esp
    call scheduler_tick_context
    add esp, 4

    ; Switch to the ESP selected by the scheduler.
    mov esp, eax

    ; Send End Of Interrupt to the master PIC.
    mov al, 0x20
    out 0x20, al

    ; Restore the selected process's registers.
    popad

    ; Return from the hardware interrupt.
    iretd
