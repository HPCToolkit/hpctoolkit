#include <regdef.h>
#include <asm.h>




	.text
	.arch generic
	.align 4

	.globl libcall1
	.ent libcall1

	# declare saved registers.  these are simple assembly
	# procedures, so we do not save anything.
	.frame sp, 0, ra

libcall1:
	# probably not strictly necessary
	ldgp gp, 0(pv)

	.prologue 1

	# receive the procedure address in $a0
	mov a0, pv

	# move the procedure arguments "up" one register level
	mov a1, a0
	

	# jump to the library function
	jmp (pv)

	.end libcall1

	.globl libcall1_end
	.ent libcall1_end
libcall1_end:
	.end libcall1_end

	.globl libcall2
	.ent libcall2

	# declare saved registers.  these are simple assembly
	# procedures, so we do not save anything.
	.frame sp, 0, ra

libcall2:
	# probably not strictly necessary
	ldgp gp, 0(pv)

	.prologue 1

	# receive the procedure address in $a0
	mov a0, pv

	# move the procedure arguments "up" one register level
	mov a1, a0
	mov a2, a1
	

	# jump to the library function
	jmp (pv)

	.end libcall2

	.globl libcall2_end
	.ent libcall2_end
libcall2_end:
	.end libcall2_end

	.globl libcall3
	.ent libcall3

	# declare saved registers.  these are simple assembly
	# procedures, so we do not save anything.
	.frame sp, 0, ra

libcall3:
	# probably not strictly necessary
	ldgp gp, 0(pv)

	.prologue 1

	# receive the procedure address in $a0
	mov a0, pv

	# move the procedure arguments "up" one register level
	mov a1, a0
	mov a2, a1
	mov a3, a2
	

	# jump to the library function
	jmp (pv)

	.end libcall3

	.globl libcall3_end
	.ent libcall3_end
libcall3_end:
	.end libcall3_end

	.globl libcall4
	.ent libcall4

	# declare saved registers.  these are simple assembly
	# procedures, so we do not save anything.
	.frame sp, 0, ra

libcall4:
	# probably not strictly necessary
	ldgp gp, 0(pv)

	.prologue 1

	# receive the procedure address in $a0
	mov a0, pv

	# move the procedure arguments "up" one register level
	mov a1, a0
	mov a2, a1
	mov a3, a2
	mov a4, a3
	

	# jump to the library function
	jmp (pv)

	.end libcall4

	.globl libcall4_end
	.ent libcall4_end
libcall4_end:
	.end libcall4_end
