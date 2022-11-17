.equ MSR_EFER		, 0xC0000080
.equ EFER_LM		, 0x100
.equ EFER_LMA		, (0x1 << 10)	
.equ EFER_NX		, 0x800
.equ EFER_SCE		, 0x001
.equ CR0_PAGING		, 0x80000000
.equ CR4_PAE		, 0x20
.equ CR4_PSE		, 0x10
.equ PG_PRESENT		, 0x1
.equ PG_WRITABLE	, 0x2
.equ PG_USER		, 0x4
.equ PG_BIG		, 0x80
.equ PG_PCD             , 0x10 // page cache disable
.equ PG_NO_EXEC		, 0x8000000000000000
.equ LOG_TABLE_SIZE	, 9
.equ LOG_PAGE_SIZE	, 12
.equ PAGE_SIZE		, (1 << LOG_PAGE_SIZE)
.equ TABLE_SIZE		, (1 << LOG_TABLE_SIZE)
.equ PRESENT,             (1)
.equ RING0,               (0)
.equ RING3,               (3)	
.equ INTERRUPT_GATE,      (0xe)	
.equ EXCEPTION_GATE,      (0xf)	
.equ SERIAL_PORT,         (0x3f8)
.equ PORT_HLT,            (0x3fa)	
.equ PORT_MY_ID,          (0x3fc)
	
.equ IST,         	  (0x0) // interrupt stack table


.equ APIC_ID_REG,          (0x20)
.equ APIC_SPUR_INT_REG,    (0xF0)
.equ APIC_ESR_REG,         (0x280)
.equ APIC_ICR_LREG,        (0x300)
.equ APIC_ICR_HREG,        (0x310)
//.equ APIC_ID_REG          (0x20)
//.equ APIC_ID_REG          (0x20)

	
.macro INT_GATE_32 offset
	.long (\offset & 0xFFFF) | (CODE_DESC - NULL_DESC) << 16
	.long ((\offset & 0xFFFF0000) << 16) | (PRESENT << 15) | (RING0 << 13) | (INTERRUPT_GATE << 8)
.endm
.macro EXPT_GATE_32 offset
	.long (\offset & 0xFFFF) | (CODE_DESC - NULL_DESC) << 16
	.long ((\offset & 0xFFFF0000) << 16) | (PRESENT << 15) | (RING0 << 13) | (EXCEPTION_GATE << 8)
.endm

.macro  IDT_TABLE from, to
	pushl %eax
	movl $\from, %eax
	shll $16, %eax
	orl $0x2, %eax
	xchg %eax, (%esp)
	jmp int_expt_handler
.if     \to-\from
	IDT_TABLE     "(\from+1)",\to
.endif
.endm

.globl no_err_code, no_err_code_sz
.extern STACK_START
.text
.globl __reset
.code16
__reset:
	cli
	//movl $(0x18000), %esp       // why -16 will think later
	//movl %esp, %ebp
	//call check_cpuid
	movl $0b10100000, %eax             // Set the PAE and PGE bit.
	movl %eax, %cr4
	movl $boot_p4, %eax
	movl %eax, %cr3
	movl $(MSR_EFER), %ecx             // Read from the EFER MSR. 
	rdmsr
	orl $(EFER_LM | EFER_NX | EFER_SCE), %eax  // Set the LME bit.
	wrmsr
	movl %cr0, %eax
	orl $0x80000001, %eax // enable paging and protected mode 
	movl %eax, %cr0 // now we are in compatibility mode
	movl $gdtr64, %eax
	lgdt (%eax)
	jmp $0x08, $long_mode	// switch to long mode

// esi has address, edi has size	
	
check_cpuid:
	// Check if CPUID is supported by attempting to flip the ID bit (bit 21) in
	// the FLAGS register. If we can flip it, CPUID is available.
	// Copy FLAGS in to EAX via stack
	pushf
	pop %eax
 
	// Copy to ECX as well for comparing later on
	mov  %eax, %ecx
 
	// Flip the ID bit
	xor $(1 << 21), %eax
 
	// Copy EAX to FLAGS via the stack
	push %eax
	popf
 
	// Copy FLAGS back to EAX (with the flipped bit if CPUID is supported)
	pushf
	pop %eax
 
	// Restore FLAGS from the old version stored in ECX (i.e. flipping the ID bit
	// back if it was ever flipped).
	push %ecx
	popf
 
	// Compare EAX and ECX. If they are equal then that means the bit wasn't
	// flipped, and CPUID isn't supported.
	xor %eax, %ecx
	jz .NoCPUID
	jmp detect_long_mode

.NoCPUID:
	hlt
.NoLongMode:
	hlt
.NoLongMode1:
	hlt
	
detect_long_mode:
	mov $0x80000000, %eax      // Set the A-register to 0x80000000.
	cpuid                  	   // CPU identification.
	cmp $0x80000001, %eax      // Compare the A-register with 0x80000001.
	jb .NoLongMode             // It is less, there is no long mode.
	mov $0x80000001, %eax      // Set the A-register to 0x80000001.
	cpuid                      // CPU identification.
	test $(1 << 29), %edx      // Test if the LM-bit, which is bit 29, is set in the D-register.
	jz .NoLongMode1            // They aren't, there is no long mode.
	ret
	
.code64

enable_sse:
	pushq %rax
	// setup sse
	movq %cr0, %rax
	and $0xFFFB, %ax          // clear coprocessor emulation CR0.EM
	or $0x2, %ax              // set coprocessor monitoring  CR0.MP
	movq %rax, %cr0

	movq %cr4, %rax
        or $(3 << 9), %ax         // set CR4.OSFXSR and CR4.OSXMMEXCPT at the same ti$
        movq %rax, %cr4

        popq %rax
        retq

// stack based arguments are not supported right now TBD	
.extern syscall_entry	
.globl __syscall_entry
__syscall_entry:
	movq    %rax, %rbp
	movq    %rdx, %rsp
	movw    $PORT_MY_ID, %dx
	xorq    %rax, %rax
	inb     %dx, %al	
	movq    $kern_stack, %rdx // store power on value, in its own id
	leaq    (%rdx, %rax, 8), %rax
	movq    (%rax), %rdx
	xchgq   %rdx, %rsp
	movq    %rbp, %rax
	movq    %rsp, %rbp
	jmp     syscall_entry
	
.globl user_trampoline, user_trampoline_sz
user_trampoline:
	.quad 0x0000000000000000
	endbr64
	movq -0x13(%rip), %rax
	callq *%rax
	movq $0xa, %rax
	syscall
user_trampoline_end:
user_trampoline_sz = user_trampoline_end - user_trampoline
// void jump_usermode(void *rip, void *stack);	
.globl jump_usermode	
jump_usermode:
	movq $(0x20 | 3), %rax // user data segment
	pushq %rax
	pushq %rsi     // stack address
	movq $0x202, %rax // RFLAGS, (Interrupt Enabled)
	pushq %rax
	//pushfq
	movq $(0x18 | 3), %rax // user code segment
	pushq %rax
	pushq %rdi // RIP
	iretq
	hlt
.globl reload_gdt
reload_gdt:
	movq $gdtr64, %rax
	lgdt (%rax)	
	pushq $0x08                 // Push code segment to stack, 0x08 is a stand-in for your code segment
	lea (.reload_gdt), %rax 
	pushq %rax                  // Push this value to the stack
	retfq                       // Perform a far return, RETFQ or LRETQ depending on syntax
	nop
	nop
.reload_gdt:
   // Reload data segment registers
	movw   $0x10, %ax  // 0x10 is a stand-in for your data segment
	movw   %ax,   %ds 
	movw   %ax,   %es 
	movw   %ax,   %fs 
	movw   %ax,   %gs 
	movw   %ax,   %ss
	retq
	
// 5th segment is the tss segment
.globl flush_tss
flush_tss:
	//mfence	
	//call reload_gdt
	addq $5, %rdi
	shlq $3, %rdi
	movq %rdi, %rax
	ltr %ax
	retq
// we use just rax, rdx we will discard	
.globl tsc
tsc:	
	rdtscp
	retq
//  left-to-right: %rdi, %rsi, %rdx, %rcx, %r8, and %r9.
// preserve rsp, rbp, rbx, r12-r15	
.globl _putchar	
_putchar:
	movq %rdi, %rax
	movw $SERIAL_PORT, %dx
	out  %al, (%dx)
	retq
// rsi has address, rdi has size	
hex_dump_64:
	pushq %rax
	pushq %rdx
	addq %rsi, %rdi
	movw $SERIAL_PORT, %dx
.loop2:
	cmpq %rsi, %rdi
	jz .halt2
	movb (%rsi), %al
	out %al, (%dx)
	incq %rsi
	jmp .loop2
.halt2:
	movb $0xaa, %al
	out %al, (%dx)
	movb $0xbb, %al
	out %al, (%dx)
	movb $0xcc, %al
	out %al, (%dx)
	movb $0xdd, %al
	out %al, (%dx)	
	popq %rdx
	popq %rax
	ret

fill_idt:
	pushq %rbp
	movq %rsp, %rbp
	subq $24, %rsp
	movq $0, %rax
	movq $idt, %rbx	// where to store
	movq $int_expt_trampoline, %rcx
.loop1:
	cmpq $256, %rax
	jz .halt1
	movl %ecx, %edx
	shll $16, %edx
	shrl $16, %edx
	orl %edx, (%rbx)
	addq $4, %rbx
	movq %rcx, %rdx
	shrq $16, %rdx
	shlq $16, %rdx
	orq %rdx, (%rbx)
	addl $1, %eax
	addq $12, %rbx
	addq $17, %rcx
	jmp .loop1
.halt1:
	//mov $idt, %esi
	//mov $32, %edi
	//call hex_dump_32
	movq %rbp, %rsp
	popq %rbp
	ret

.macro OUTB port, value
	movb $\value, %al
	movw $\port, %dx
	out  %al, (%dx)			
.endm	
disable_pic:
	OUTB 0x20, 0x11
	OUTB 0xa0, 0x11
	OUTB 0x21, 0xe0
	OUTB 0xa1, 0xe8
        OUTB 0x21, 4
        OUTB 0xa1, 2
       /* Set ICW4 */
        OUTB 0x21, 1
        OUTB 0xa1, 1
        /* Set OCW1 (interrupt masks) */
        OUTB 0x21, 0xff
        OUTB 0xa1, 0xff
	retq

/*
    APIC Base   :  0xFEE0_0000 (MSR 0x0000_001B)
    0x20	: APIC ID
    0x80	: task priority register
    0xA0	: processor priority register
    0xB0	: EOI (we must write zero here)
    0xC0	: Remote read register
    0xD0	: Logical destination register
    0xE0	: destination format register
    0xF0	: spurious interrupt vector
    100-170h	: In-Service Register (ISR)
    180-1F0h	: Trigger Mode Register (TMR)
    200-270h	: Interrupt Request Register (IRR)
    280h	: Error Status Register (ESR)
    300h	: Interrupt Command Register Low (bits 31:0)
    310h	: Interrupt Command Register High (bits 63:32)
    320h	: Timer Local Vector Table Entry
    330h	: Thermal Local Vector Table Entry
    340h	: Performance Counter Local Vector Table Entry
    350h	: Local Interrupt 0 Vector Table Entry
    360h	: Local Interrupt 1 Vector Table Entry
    370h	: Error Vector Table Entry
    380h	: Timer Initial Count Register
    390h	: Timer Current Count Register
    3E0h	: Timer Divide Configuration Register
    400h	: Extended APIC Feature Register
    410h	: Extended APIC Control Register
    420h	: Specific End of Interrupt Register (SEOI)
    480-4F0h	: Interrupt Enable Registers (IER)
    500-530h	: Extended Interrupt [3:0] Local Vector Table Registers	
*/	
// assume ebx has base address
.macro OR_APIC_REG OFFSET, VALUE, BIT_POS
	movl    %ebx, %eax
	addl    $\OFFSET, %eax
	movl    $\VALUE, %ecx
	shll    $\BIT_POS, %ecx
	orl     %ecx, (%eax)
.endm

.macro WRITE_APIC_REG OFFSET, VALUE
	movl    %ebx, %eax
	addl    $\OFFSET, %eax
	movl    $\VALUE, %ecx
	movl    %ecx, (%eax)
.endm
.macro APIC_SPIN_WAIT
	movl %ebx, %eax
	addl $APIC_ICR_LREG, %eax
	pause
	testl $(1<<12), (%eax)
	jz .-9
.endm
enable_apic:
	// write 0 to error register
	movl    $(0xFEE00000), %ebx
	WRITE_APIC_REG APIC_ESR_REG, 0
	OR_APIC_REG APIC_SPUR_INT_REG, 0x3, 8
	OR_APIC_REG APIC_SPUR_INT_REG, 0xFF, 0
	OR_APIC_REG APIC_ICR_HREG, 0x01, 24
	WRITE_APIC_REG APIC_ICR_LREG, 0x4500
	WRITE_APIC_REG APIC_ESR_REG, 0
	OR_APIC_REG APIC_ICR_HREG, 0x01, 24
	WRITE_APIC_REG APIC_ICR_LREG, 0x0602
	WRITE_APIC_REG APIC_ICR_LREG, 0x0602
	//APIC_SPIN_WAIT
	retq	
/*
  Stack design for each vcpu kernel
    |                       |
    |                       | <-- stack_start - 16, actual starting
    | EMPTY                 |
    | POOL<<8 | PROC_ID     | <--- stack start
*/
	
long_mode:
	// Set up the stack pointer and call into C.
	subq    $16, %rsp
	movw    $PORT_MY_ID, %dx
	xor     %rax, %rax
	inw     %dx, %ax
	movq    $kern_stack, %rbx // store power on value, in its own id
	movl    %eax, %ecx
	andq    $0x00FF, %rcx
	leaq    (%rbx, %rcx, 8), %rbx
	movq    %rsp, (%rbx)
	movq    %rsp, %rbp
	shrq    $12, %rbp  // page aligned address
	addq    $1, %rbp
	shlq    $12, %rbp
	movq    %rax, (%rbp)
	movq 	%rsp, %rbp
        movq    $(DATA_DESC_64 - NULL_DESC_64), %rax    // Our data segment selector
        movq    %rax, %ds      // -> DS: Data Segment
        movq    %rax, %es      // -> ES: Extra Segment
        movq    %rax, %ss      // -> SS: Stack Segment
	call 	enable_sse
	jmp     fill_idt_lock_grab_code
fill_idt_lock_grab:
	.quad	0x1
fill_idt_lock_grab_code:	
	xor     %rax, %rax
	movq    $fill_idt_lock_grab, %rbx
	xchgq   %rax, (%rbx)
	testq   $0x1, %rax
	jz      wait_for_fill_idt
	//movw $SERIAL_PORT, %dx
	//movb $'#', %al
	//out  %al, (%dx)
	//movb $'\n', %al
	//out  %al, (%dx)		
	call    fill_idt
	call disable_pic
	call enable_apic	
// dont progress ahead until IDT's have been filled	
	movq    $fill_idt_lock_grab, %rbx
	movq    $2, (%rbx)
	jmp     skip_wait_for_fill_idt
wait_for_fill_idt:	
	movq    $fill_idt_lock_grab, %rbx	
	testq   $2, (%rbx)
	pause   // TBD idea is not to wait in busy loop	
	jne wait_for_fill_idt
	//movq 	$idt, %rsi
	//movq 	$0x40, %rdi
	//call 	hex_dump_64
skip_wait_for_fill_idt:	
        movq    $idt_descriptor, %rax
        lidtq   (%rax)	
	callq   main
	hlt

// rbx, rbp, r12-r15 are callee saved registers
.macro SAVE_CONTEXT
	pushq %rax
	pushq %rcx
	pushq %rdx
	pushq %rsi
	pushq %rdi
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
.endm

.macro RESTORE_CONTEXT
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rdi
	popq %rsi
	popq %rdx
	popq %rcx
	popq %rax
	
.endm	
.globl high_interrupt_handler	
int_expt_handler_64:
	cli
	pushq %rax
	pushq %rbx
	movq 16(%rsp), %rbx
	movq $no_err_code, %rax
	cmpb $0, (%rax, %rbx)
	popq %rbx
	popq %rax
	je jmp2high_interrupt
	xchgq %rax, -16(%rsp)
	movq (%rsp), %rax
	movq $0xdead, (%rsp)
	pushq %rax
	xchgq %rax, -8(%rsp)
jmp2high_interrupt:
SAVE_CONTEXT
	leaq 72(%rsp), %rdi
	callq high_interrupt_handler
RESTORE_CONTEXT
	addq $16, %rsp
	nop
	nop
	// iret must know stack is 64 bit return. amd-vol2-8.9.5
	iretq	
.section ".data"	
.p2align 4
// the global descriptor table
.globl gdt64_start_c
gdt64_start_c:
gdt64:
NULL_DESC_64:	
	.quad 0
CODE_DESC_64:	
	.quad 0x00AF98000000FFFF
DATA_DESC_64:
	.quad 0x00CF92000000FFFF
	.quad 0x0000000000000000 // user code seg
	.quad 0x0000000000000000 // user data seg
	.fill 512, 8, 0	 // tss seg it is 16 bytes
	
gdt64_end:
.p2align 4
gdtr64:
	.word gdt64_end - gdt64 - 1
	.quad gdt64

// no error code
no_err_code:	
	.byte \
	1, 1, 1, 1, 1, 1, 1, 1,\
	0, 1, 0, 0, 0, 0, 0, 1,\
	1, 0, 1, 1, 1, 0, 1, 1
.fill 232, 1, 1
no_err_code_sz = . - no_err_code
	
// maybe this is useless? delete it later
scratch_pad:
.rept 512
	.byte 0
.endr

.section .interrupt_table, "aw", @progbits

.macro  IDT_TABLE_64 from, to
	pushq %rax
	movq $\from, %rax
	xchgq %rax, (%rsp)
	jmp int_expt_handler_64
.if     \to-\from
	IDT_TABLE_64     "(\from+1)",\to
.endif
.endm	
int_expt_trampoline:
IDT_TABLE_64 0, 100
IDT_TABLE_64 101, 201
IDT_TABLE_64 202, 255

.section .interrupt_descriptors, "aw", @progbits
.macro INT_GATE_64 offset
	.long (\offset & 0xFFFF) | (CODE_DESC_64 - NULL_DESC_64) << 16
	.long ((\offset & 0xFFFF0000) << 16) | (PRESENT << 15) | \
	(RING0 << 13) | (INTERRUPT_GATE << 8) | (IST << 0)
	.long (\offset & (0xffffffff << 32))
	.long 0
.endm

.macro R3_INT_GATE_64 offset
	.long (\offset & 0xFFFF) | (CODE_DESC_64 - NULL_DESC_64) << 16
	.long ((\offset & 0xFFFF0000) << 16) | (PRESENT << 15) | \
	(RING3 << 13) | (INTERRUPT_GATE << 8) | (IST << 0)
	.long (\offset & (0xffffffff << 32))
	.long 0
.endm
	
.macro EXPT_GATE_64 offset
	.long (\offset & 0xFFFF) | (CODE_DESC_64 - NULL_DESC_64) << 16
	.long ((\offset & 0xFFFF0000) << 16) | (PRESENT << 15) | \
	(RING0 << 13) | (EXCEPTION_GATE << 8) | (IST << 0)
	.long (\offset & (0xffffffff << 32))
	.long 0	
.endm
	
.p2align 4
idt:
	.rept 20
		EXPT_GATE_64 offset=0
	.endr
	.rept 236
		R3_INT_GATE_64 offset=0
	.endr
	
idt_end:	
idt_descriptor :
    .word idt_end - idt - 1 // Size of our idt, always one less than the actual size
    .quad idt // Start address of our idt	

.globl kern_stack	
kern_stack:
	.fill  512, 8, 0
	
// paging structures
.section .paging, "aw", @progbits
.p2align 12 // page align
.globl boot_p4
boot_p4:
        .quad (boot_p3 + PG_PRESENT + PG_WRITABLE + PG_USER)
        .fill 511, 8, 0

boot_p3:
        .quad (boot_p2 + PG_PRESENT + PG_WRITABLE + PG_USER)
	.quad 0 // 1 gb
	.quad 0 // 2
	.quad (apic_page + PG_PRESENT + PG_WRITABLE + PG_USER) // 3 gb
        .fill 508, 8, 0

boot_p2:
.set pg, 0
  .rept 512
    .quad (pg + PG_PRESENT + PG_WRITABLE + PG_BIG)
    .set pg, pg+(PAGE_SIZE * 512)
  .endr	

apic_page:
	.fill 503, 8, 0
	.quad (0xFEE00000 + PG_PRESENT + PG_WRITABLE + PG_BIG + PG_PCD)
	.fill 8, 8, 0
