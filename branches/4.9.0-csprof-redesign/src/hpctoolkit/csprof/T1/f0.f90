! try to use libunwind w fortran
!
!
program f0
real x,y,z

x = 5.0

call tfun()
call f_load_mod()
call f_unwind()

print *,'Hello world, x = ',x

end program
