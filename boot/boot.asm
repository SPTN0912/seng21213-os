; =============================================================================
; SENG21213-OS :: Stage 0 Bootloader
; File   : boot/boot.asm
; Author : SENG 21213 – Computer Architecture and Operating Systems
; Purpose: MBR (Master Boot Record) bootloader. Switches CPU from 16-bit Real
;          Mode to 32-bit Protected Mode, then loads and jumps to the kernel.
; =============================================================================

[BITS 16]           ; CPU starts in 16-bit Real Mode
[ORG 0x7C00]        ; BIOS loads the MBR at this fixed address

; ---------------------------------------------------------------------------
; Entry: Real Mode setup
; ---------------------------------------------------------------------------
start:
    cli                ; Disable interrupts during setup
    xor  ax, ax
    mov  ds, ax        ; Data Segment = 0
    mov  es, ax        ; Extra Segment = 0
    mov  ss, ax        ; Stack Segment = 0
    mov  sp, 0x7C00    ; Stack pointer just below our code
    sti                ; Re-enable interrupts

    ; Save drive number (BIOS stores it in dl)
    mov  [boot_drive], dl

    ; Print loading banner using BIOS int 0x10
    mov  si, msg_banner
    call print_rm
    mov  si, msg_load
    call print_rm

    ; Get BIOS E820 physical memory map.
    call detect_memory


; ---------------------------------------------------------------------------
; Load kernel: read sectors 2..65 from disk into memory at 0x1000:0x0000
; This gives us 64 × 512 = 32 768 bytes for the kernel (Stage 0)
; ---------------------------------------------------------------------------
load_kernel:
    mov  bx, 0x1000        ; ES:BX = 0x10000 (kernel load address)
    mov  es, bx
    xor  bx, bx

    mov  ah, 0x02          ; BIOS read sectors
    mov  al, 64            ; Number of sectors to read
    mov  ch, 0             ; Cylinder 0
    mov  cl, 2             ; Start from sector 2 (sector 1 is MBR)
    mov  dh, 0             ; Head 0
    mov  dl, [boot_drive]  ; Drive number
    int  0x13
    jc   disk_error        ; Carry flag set = error

    mov  si, msg_ok
    call print_rm

; ---------------------------------------------------------------------------
; Enter Protected Mode
; ---------------------------------------------------------------------------
enter_pm:
    cli
    lgdt [gdt_descriptor]  ; Load the Global Descriptor Table

    mov  eax, cr0
    or   eax, 0x1          ; Set PE (Protection Enable) bit
    mov  cr0, eax

    ; Far jump to flush the prefetch queue and load CS with code segment
    jmp  CODE_SEG:init_pm32

; ---------------------------------------------------------------------------
; 32-bit Protected Mode initialisation
; ---------------------------------------------------------------------------
[BITS 32]
init_pm32:
    ; Set all data segment registers to the data descriptor
    mov  ax, DATA_SEG
    mov  ds, ax
    mov  ss, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    ; Set up a proper kernel stack at 0x200000
    mov  ebp, 0x200000
    mov  esp, ebp

    ; Jump to the kernel entry point (loaded at 0x10000)
    call 0x10000

    ; Should never return, but halt if it does
    hlt

; ---------------------------------------------------------------------------
; Error handlers
; ---------------------------------------------------------------------------
[BITS 16]

disk_error:
    mov  si, msg_err
    call print_rm
    mov  si, msg_halt
    call print_rm
    jmp $
; ---------------------------------------------------------------------------
; BIOS E820 memory map
;
; Stores:
;   [0x8000]     = number of entries
;   [0x8004]     = first 24-byte E820 entry
;   [0x801C]     = second entry
;   ...
;
; Maximum: 32 entries
; ---------------------------------------------------------------------------
detect_memory:
    xor eax, eax
    mov [0x8000], eax       ; Entry count = 0

    xor ebx, ebx            ; EBX = continuation value
    mov di, 0x8004          ; ES:DI = destination buffer
    mov bp, 32               ; Maximum number of entries

.e820_loop:
    cmp bp, 0
    je .done

    mov eax, 0xE820
    mov edx, 0x534D4150     ; "SMAP"
    mov ecx, 24              ; Request 24-byte entry
    mov dword [es:di + 20], 1 ; ACPI extended attributes = enabled

    int 0x15
    jc .done                 ; BIOS error

    cmp eax, 0x534D4150      ; Verify "SMAP"
    jne .done

    cmp ecx, 20
    jb .next                  ; Invalid entry size

    ; Count this entry.
    inc dword [0x8000]

.next:
    add di, 24
    dec bp

    cmp ebx, 0
    jne .e820_loop

.done:
    ret

; ---------------------------------------------------------------------------
; Subroutine: print_rm – print NUL-terminated string in SI (Real Mode)
; ---------------------------------------------------------------------------
print_rm:
    lodsb               ; Load byte at [SI] into AL, advance SI
    or   al, al
    jz   .done
    mov  ah, 0x0E       ; BIOS teletype output
    xor  bh, bh
    int  0x10
    jmp  print_rm
.done:
    ret

; ---------------------------------------------------------------------------
; Data
; ---------------------------------------------------------------------------
boot_drive  db 0

msg_banner  db 13, 10, ' SENG21213-OS Stage 0', 13, 10, 0
msg_load    db '  [BOOT] Loading kernel...', 13, 10, 0
msg_ok      db '  [BOOT] Kernel loaded OK ', 13, 10, 0
msg_err     db '  [BOOT] DISK ERROR!       ', 13, 10, 0
msg_halt    db '  System halted.           ', 13, 10, 0

; ---------------------------------------------------------------------------
; GDT – Global Descriptor Table
; Two flat (0–4 GB) segments: Code and Data, Ring 0
; ---------------------------------------------------------------------------
gdt_start:
gdt_null:                   ; Mandatory null descriptor
    dd 0x00000000
    dd 0x00000000

gdt_code:                   ; Executable, readable, Ring 0
    dw 0xFFFF               ; Limit [15:0]
    dw 0x0000               ; Base  [15:0]
    db 0x00                 ; Base  [23:16]
    db 10011010b            ; Access byte: Present|Ring0|Type=1(code)|Exec|Read
    db 11001111b            ; Flags + Limit [19:16]: 4K granularity, 32-bit
    db 0x00                 ; Base  [31:24]

gdt_data:                   ; Readable, writable, Ring 0
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b            ; Access: Present|Ring0|Type=0(data)|Read|Write
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1   ; GDT limit (size - 1)
    dd gdt_start                  ; GDT base address

; Segment selectors (byte offset into GDT)
CODE_SEG equ gdt_code - gdt_start   ; = 0x08
DATA_SEG equ gdt_data - gdt_start   ; = 0x10

; ---------------------------------------------------------------------------
; Boot signature – BIOS checks for 0xAA55 at bytes 510-511
; ---------------------------------------------------------------------------
times 510 - ($ - $$) db 0
dw 0xAA55
