double precision function msin(xx)

  double precision :: x,xx
  integer :: i

   x = xx
   do i=1,1000
      x = x + 1.
   enddo

   msin = xx

end

double precision function mcos(x)
  double precision :: x
  double precision :: y, msin
  y = msin(x)
  mcos= x
end

double precision function mlog(x)
  double precision :: x
  double precision :: y,mcos
  y = mcos(x)
  mlog= x
end

subroutine foob(x)
  double precision :: x,tmp
  x = x * 3.14 + log(x)
end

!program f1
subroutine f1()
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
