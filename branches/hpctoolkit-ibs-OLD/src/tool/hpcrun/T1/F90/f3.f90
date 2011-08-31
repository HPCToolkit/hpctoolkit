subroutine foob(x)
  double precision :: x,tmp
  x = x * 3.14 + log(x)
end

program f1
  double precision :: x,y,z
!  double precision :: msin, mlog, mcos
  integer :: i,j

  integer,parameter :: LIMIT_OUTER = 1000
  integer,parameter :: LIMIT       = 10000

  x = 2.78
  call foob(x)
  y = 0.
  do i=1,LIMIT_OUTER
     do j=1,LIMIT
        y = x * x + sin(y)
        x = log(y) + cos(x)
     enddo
  enddo
  print *,'final: x = ',x,', y = ',y
 
end
