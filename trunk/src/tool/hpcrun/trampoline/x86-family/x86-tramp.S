#include <include/hpctoolkit-config.h>
#if defined(HOST_CPU_x86_64)
#  define x86_Reg_BP rbp
#  define x86_Reg_DX rdx
#  define x86_Reg_CX rcx
#  define x86_Reg_BX rbx
#  define x86_Reg_AX rax
#  define x86_Reg_SP rsp
#  define x86_Reg_SI rsi
#  define x86_Reg_DI rdi
#elif defined (HOST_CPU_x86)
#  define x86_Reg_BP ebp
#  define x86_Reg_DX edx	
#  define x86_Reg_CX ecx
#  define x86_Reg_BX ebx
#  define x86_Reg_AX eax
#  define x86_Reg_SP esp
#  define x86_Reg_SI esi
#  define x86_Reg_DI edi
#else
#  error "Unknown architecture (HOST_CPU)!"
#endif
	.text
	.global hpcrun_trampoline_handler
	.type hpcrun_trampoline_handler,@function

	.text
	.global hpcrun_trampoline
	.type hpcrun_trampoline,@function	
hpcrun_trampoline:
	push	%x86_Reg_BP /* need to save a slot for the "real" return address */

	/* regular standard frame here */

	push	%x86_Reg_BP
	mov	%x86_Reg_SP, %x86_Reg_BP

	/* save return value and callee-save registers in approximately
	   the same order which <bits/sigcontext.h> defines them in.
	   this may be good if someday we decide to index things off of
	   the base pointer (unlikely, but who knows?).
	   save return value registers */
	

	push	%x86_Reg_CX
	push	%x86_Reg_BX
	push	%x86_Reg_AX
	push	%x86_Reg_DX
	push	%x86_Reg_SI
	push	%x86_Reg_DI
#if defined(HOST_CPU_x86_64)	
	push	%r15
	push	%r14
	push	%r13
	push	%r12
	push	%r11
	push	%r10
	push	%r9
	push	%r8
#endif

	/* move stack pointer into first argument register */
	mov	%x86_Reg_SP, %x86_Reg_DI

	/* away we go into C-land */
	call hpcrun_trampoline_handler

	/* the trampoline returns us our proper return address in %?ax.
	   shove it onto the stack so that 'ret' will go to the proper
	   location upon returning from this function. */

	mov	%x86_Reg_AX, SIZEOF_VOIDP(%x86_Reg_BP)

	/* pop all the registers back into place */
#if defined(HOST_CPU_x86_64)
	pop	%r8
	pop	%r9
	pop	%r10
	pop	%r11
	pop	%r12
	pop	%r13
	pop	%r14
	pop	%r15
#endif
	pop	%x86_Reg_DI
	pop	%x86_Reg_SI
	pop	%x86_Reg_DX
	pop	%x86_Reg_AX
	pop	%x86_Reg_BX
	pop	%x86_Reg_CX

	/* Head back into the normal program */
	leave
	/* jmp	*%rcx */

	.size hpcrun_trampoline,.-hpcrun_trampoline

	.text
	/* never actually gets called; used to determine if the current
	   pc is in trampoline code of some kind */
	.global hpcrun_trampoline_end
        .type hpcrun_trampoline_end,@function
hpcrun_trampoline_end:
	ret
