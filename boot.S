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
	movl $(STACK_START-16), %esp // why -16 will think later
	movl %esp, %ebp
	call check_cpuid
//fill_idt:
//	pushl %ebp
//	movl %esp, %ebp
//	subl $12, %esp
//	movl $0, %eax
//	movl $idt, %ebx	// where to store
//	movl $int_expt_trampoline, %ecx
//.loop1:
//	cmp $256, %eax
//	jz .halt1
//	mov %ecx, %edx
//	shll $16, %edx
//	shrl $16, %edx
//	orl %edx, (%ebx)
//	addl $4, %ebx
//	mov %ecx, %edx
//	shrl $16, %edx
//	shll $16, %edx
//	orl %edx, (%ebx)
//	addl $1, %eax
//	addl $4, %ebx
//	addl $20, %ecx
//	jmp .loop1
//.halt1:
//	//mov $idt, %esi
//	//mov $32, %edi
//	//call hex_dump_32
//	movl %ebp, %esp
//	popl %ebp
	// Enter long mode.
	movl $0b10100000, %eax                // Set the PAE and PGE bit.
	movl %eax, %cr4
	movl $boot_p4, %eax
	movl %eax, %cr3
	movl $0xC0000080, %ecx               // Read from the EFER MSR. 
	rdmsr
	orl $0x00000100, %eax                // Set the LME bit.
	wrmsr
	movl %cr0, %eax
	orl $0x80000001, %eax
	movl %eax, %cr0
	movl $gdtr64, %eax
	lgdt (%eax)
	jmp $0x08, $long_mode	
// esi has address, edi has size	
hex_dump_16:
	push %eax
	push %edx
	addl %esi, %edi
	movw $SERIAL_PORT, %dx
.loop0:
	cmp %esi, %edi
	jz .halt0
	movb (%esi), %al
	out %al, (%dx)
	inc %esi
	jmp .loop0
.halt0:
	movb $0xaa, %al
	out %al, (%dx)
	movb $0xbb, %al
	out %al, (%dx)
	movb $0xcc, %al
	out %al, (%dx)
	movb $0xdd, %al
	out %al, (%dx)	
	pop %edx
	pop %eax
	ret

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
	cmp $0x80000001, %eax     // Compare the A-register with 0x80000001.
	jb .NoLongMode             // It is less, there is no long mode.
	mov $0x80000001, %eax    // Set the A-register to 0x80000001.
	cpuid                      // CPU identification.
	test $(1 << 29), %edx      // Test if the LM-bit, which is bit 29, is set in the D-register.
	jz .NoLongMode1             // They aren't, there is no long mode.
	ret
int_expt_trampoline:
IDT_TABLE 0, 100
	
int_expt_handler:
	cli
	movl %esp, %esi
	movl $20, %edi
	call hex_dump_16
	popl %eax
	popl %eax
	iret


	
	
.p2align 4
idt:
	.rept 20
		EXPT_GATE_32 offset=0
	.endr
	.rept 236
		INT_GATE_32 offset=0
	.endr
	
idt_end:	
idt_descriptor :
    .word idt_end - idt - 1 // Size of our idt, always one less than the actual size
    .quad idt // Start address of our idt

.p2align 4
NULL_DESC:
	.quad 0x0000000000000000

CODE_DESC:
	.word 0xFFFF       // limit low
	.word 0            // base low
	.byte 0            // base middle
	.byte 0b10011010    // access
	.byte 0b11001111    // granularity
	.byte 0            // base high

DATA_DESC:
	.word 0xFFFF       // data descriptor
	.word 0            // limit low
	.byte 0            // base low
	.byte 0b10010010    // access
	.byte 0b11001111    // granularity
	.byte 0            // base high
gdtr:
	.word gdtr - NULL_DESC - 1
	.long NULL_DESC
	
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
long_mode:
trampoline64:
// Set up the stack pointer and call into C.
	movq    $(STACK_START-16), %rsp
	movq 	%rsp, %rbp
	call 	enable_sse
	hlt
	
	
.p2align 4
# the global descriptor table
gdt64:
	.quad 0
	.quad 0x00AF98000000FFFF
	.quad 0x00CF92000000FFFF
gdt64_end:
.p2align 4
gdtr64:
	.word gdt64_end - gdt64 - 1
	.quad gdt64

# paging structures
#	.section .paging
.section .text
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
