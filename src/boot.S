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
.equ PG_NO_EXEC		, 0x8000000000000000
.equ LOG_TABLE_SIZE	, 9
.equ LOG_PAGE_SIZE	, 12
.equ PAGE_SIZE		, (1 << LOG_PAGE_SIZE)
.equ TABLE_SIZE		, (1 << LOG_TABLE_SIZE)
.equ PRESENT,             (1)
.equ RING0,               (0)
.equ INTERRUPT_GATE,      (0xe)	
.equ EXCEPTION_GATE,      (0xf)	
.equ SERIAL_PORT,         (0x3f8)
.equ IST,         	  (0x0) // interrupt stack table
	
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
	
.extern STACK_START
.text
.globl __reset
.code16
__reset:
	cli
	movl $(STACK_START-16), %esp       // why -16 will think later
	movl %esp, %ebp
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
.macro INT_GATE_64 offset
	.long (\offset & 0xFFFF) | (CODE_DESC_64 - NULL_DESC_64) << 16
	.long ((\offset & 0xFFFF0000) << 16) | (PRESENT << 15) | \
	(RING0 << 13) | (INTERRUPT_GATE << 8) | (IST << 0)
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

.macro  IDT_TABLE_64 from, to
	pushq %rax
	movq $\from, %rax
	shlq $16, %rax
	orq $0x2, %rax
	xchgq %rax, (%rsp)
	jmp int_expt_handler_64
.if     \to-\from
	IDT_TABLE_64     "(\from+1)",\to
.endif
.endm

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

// input in in rdi
.globl flush_tss
flush_tss:
	ltr (%rdi)
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
	addq $25, %rcx
	jmp .loop1
.halt1:
	//mov $idt, %esi
	//mov $32, %edi
	//call hex_dump_32
	movq %rbp, %rsp
	popq %rbp
	ret
	
long_mode:
        // Set up the stack pointer and call into C.
	movq    $(STACK_START-16), %rsp
	movq 	%rsp, %rbp
        movq    $(DATA_DESC_64 - NULL_DESC_64), %rax    // Our data segment selector
        movq    %rax, %ds      // -> DS: Data Segment
        movq    %rax, %es      // -> ES: Extra Segment
        movq    %rax, %ss      // -> SS: Stack Segment
	call 	enable_sse
	call    fill_idt
	//movq 	$idt, %rsi
	//movq 	$0x40, %rdi
	//call 	hex_dump_64
        movq    $idt_descriptor, %rax
        lidtq   (%rax)
	callq   main
	hlt

	

.section ".data"	
.p2align 4
// the global descriptor table
.globl __gdt64_start	
__gdt64_start:
gdt64:
NULL_DESC_64:	
	.quad 0
CODE_DESC_64:	
	.quad 0x00AF98000000FFFF
DATA_DESC_64:
	.quad 0x00CF92000000FFFF
gdt64_end:
.p2align 4
gdtr64:
	.word gdt64_end - gdt64 - 1
	.quad gdt64
.fill 16, 8, 0
	
// paging structures
// .section .paging
.p2align 12
.globl boot_p4
boot_p4:
        .quad (boot_p3 + PG_PRESENT + PG_WRITABLE)
        .fill 511, 8, 0

boot_p3:
        .quad (boot_p2 + PG_PRESENT + PG_WRITABLE)
        .fill 511, 8, 0

boot_p2:
.set pg, 0
  .rept 512
    .quad (pg + PG_PRESENT + PG_WRITABLE + PG_BIG)
    .set pg, pg+(PAGE_SIZE * 512)
  .endr

scratch_pad:
.rept 512
	.byte 0
.endr

	
int_expt_trampoline:
IDT_TABLE_64 0, 100
IDT_TABLE_64 101, 201
IDT_TABLE_64 202, 255	
int_expt_handler_64:
	cli
	movq %rsp, %rsi
	movq $48, %rdi
	call hex_dump_64	
	popq %rax
	// iret must know stack is 64 bit return. amd-vol2-8.9.5
	iretq

.p2align 4
idt:
	.rept 20
		EXPT_GATE_64 offset=0
	.endr
	.rept 236
		INT_GATE_64 offset=0
	.endr
	
idt_end:	
idt_descriptor :
    .word idt_end - idt - 1 // Size of our idt, always one less than the actual size
    .quad idt // Start address of our idt	
