#define ASM_FILE        1
#include <multiboot2.h>
#define STACK_SIZE                      0x4000

/*whole code is dumped in .text section, we jump to multiboot_entry so we can keep writing values at the current offset position from the start of .text*/

        .text

        .globl  start, _start
start:
_start:
        jmp     multiboot_entry

        /*  Align 64 bits boundary. */
        .align  8
        
        /*  Multiboot header. */
multiboot_header:
        /*  magic */
        .long   MULTIBOOT2_HEADER_MAGIC
        /*  ISA: i386 */
        .long   MULTIBOOT_ARCHITECTURE_I386
        /*  Header length. */
        .long   multiboot_header_end - multiboot_header
        /*  checksum */
        .long   -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + (multiboot_header_end - multiboot_header))

		.align 8 
framebuffer_tag_start:  
        .short MULTIBOOT_HEADER_TAG_FRAMEBUFFER
        .short MULTIBOOT_HEADER_TAG_OPTIONAL
        .long framebuffer_tag_end - framebuffer_tag_start
        .long 1024
        .long 768
        .long 32


		.align 8 
framebuffer_tag_end:
        .short MULTIBOOT_HEADER_TAG_END
        .short 0
        .long 8

multiboot_header_end:
multiboot_entry:
        /*  Initialize the stack pointer. */
        movl    $(stack + STACK_SIZE), %esp

        /*  Reset EFLAGS. */
        pushl   $0
        popf

				/*following cdecl caller responsibilites here for stack, args in reverse order, then calling function*/
				/*callee pushes ebp value of caller onto stack and writes esp into ebp*/
        /*  Push the pointer to the Multiboot information structure. */
        pushl   %ebx
        /*  Push the magic value. */
        pushl   %eax

        /*  Now enter the C main function... */
        call    kernel_main

        /*  Halt. */
loop:   hlt
        jmp     loop

halt_message:
        .asciz  "Halted."

        /*  Our stack area. */
        .comm   stack, STACK_SIZE
