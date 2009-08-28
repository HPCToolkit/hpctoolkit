	.text
	.extern csprof_trampoline2

	.global csprof_trampoline
	.type csprof_trampoline,@function
csprof_trampoline:
        br.ret.sptk.many b0
        
	# never actually gets called; used to determine if the current
	# pc is in trampoline code of some kind
	.global csprof_trampoline_end
        .type csprof_trampoline_end,@function
csprof_trampoline_end:
        br.ret.sptk.many b0
