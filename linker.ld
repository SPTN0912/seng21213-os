/* =============================================================================
 * SENG21213-OS :: Linker Script
 * File   : linker.ld
 *
 * Places the kernel binary starting at physical address 0x10000.
 * ============================================================================*/

OUTPUT_FORMAT(elf32-i386)

ENTRY(_start)

/*
 * Define separate loadable segments:
 *   text = Read + Execute
 *   data = Read + Write
 */
PHDRS
{
    text PT_LOAD FLAGS(5);    /* R + X */
    data PT_LOAD FLAGS(6);    /* R + W */
}

SECTIONS
{
    . = 0x10000;

    .text : ALIGN(16)
    {
        *(.text)
    } :text

    .rodata : ALIGN(16)
    {
        *(.rodata*)
    } :text

    .data : ALIGN(16)
    {
        *(.data)
    } :data

    .bss : ALIGN(16)
    {
        *(COMMON)
        *(.bss)
    } :data

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.comment)
    }
}
