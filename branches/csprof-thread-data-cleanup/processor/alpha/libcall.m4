#include <regdef.h>
#include <asm.h>
define(`asmprelude',`
	.text
	.arch generic
	.align 4')
define(`move_args_loop',
       `ifelse(`$1', `$2',,
`mov a$1, a`'decr($1)
	move_args_loop(incr($1), $2)')')dnl
dnl takes one argument--the number of arguments which the library function
dnl expects.  generates the assembly code required.
define(`deflibcall', `
	.globl libcall$1
	.ent libcall$1

	# declare saved registers.  these are simple assembly
	# procedures, so we do not save anything.
	.frame sp, 0, ra

libcall$1:
	# probably not strictly necessary
	ldgp gp, 0(pv)

	.prologue 1

	# receive the procedure address in $a0
	mov a0, pv

	# move the procedure arguments "up" one register level
	move_args_loop(1, `incr($1)')

	# jump to the library function
	jmp (pv)

	.end libcall$1

	.globl libcall$1_end
	.ent libcall$1_end
libcall$1_end:
	.end libcall$1_end')

asmprelude
deflibcall(1)
deflibcall(2)
deflibcall(3)
deflibcall(4)
