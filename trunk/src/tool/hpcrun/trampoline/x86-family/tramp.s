	.text
	.global hpcrun_trampoline_handler
	.type hpcrun_trampoline_handler,@function

	.text
	.global hpcrun_trampoline
	.type hpcrun_trampoline,@function	
hpcrun_trampoline:
	push	%rbp
	mov	%rsp, %rbp

	# save return value and callee-save registers in approximately
	# the same order which <bits/sigcontext.h> defines them in.
	# this may be good if someday we decide to index things off of
	# the base pointer (unlikely, but who knows?).
	# save return value registers
	
	# FIXME: this is probably inefficient...saving more than we need
	push	%rbx
	push	%rax
	push	%rdx
	push	%rsi
	push	%rdi
	push	%r15
	push	%r14
	push	%r13
	push	%r12
	push	%r11
	push	%r10
	push	%r9
	push	%r8

	# move stack pointer into first argument register
	mov	%rsp, %rdi

	# away we go into C-land
	call hpcrun_trampoline_handler

	# the trampoline returns us our proper return address in %rax.
	# shove it onto the stack so that 'ret' will go to the proper
	# location upon returning from this function.
	mov	%rax, %rcx

	# pop all the registers back into place
	pop	%r8
	pop	%r9
	pop	%r10
	pop	%r11
	pop	%r12
	pop	%r13
	pop	%r14
	pop	%r15
	pop	%rdi
	pop	%rsi
	pop	%rdx
	pop	%rax
	pop	%rbx

	# Head back into the normal program
	leave
	jmp	%rcx

	.size hpcrun_trampoline,.-hpcrun_trampoline

	.text
	# never actually gets called; used to determine if the current
	# pc is in trampoline code of some kind
	.global hpcrun_trampoline_end
        .type hpcrun_trampoline_end,@function
hpcrun_trampoline_end:
	ret
