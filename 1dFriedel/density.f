  subroutine density(E_c,kbT,Nx1)
  use params
  implicit none
  integer, parameter :: UN = 11
  integer :: Imax! number of points along along x
  real(WP), intent(in) :: E_c
  real(WP), intent(in) :: kbT
  real(WP) :: Ef,hx,s,f_F,f_FF
  real(WP), allocatable  :: NxatET(:) 
  real(WP), allocatable  :: k(:,:)
  real(WP), allocatable  :: Fn(:,:)
  real(WP), intent(out)  :: Nx1(1:Nj)
!  real(WP), allocatable :: rtmp(:)
  integer ierr
  integer :: ief,iefmin,iefmax,ix
  Imax=Nj
  iEfmax = (Emax-Emin)/deltaE+1
  iEfmin=1

write (*,*) " allocating ", Nj*iEfmax*8," bytes"
  allocate(Fn(1:Nj,1:iEfmax))
write (*,*) " ok"

write (*,*) " allocating ", Nj*iEfmax*8," bytes"
  allocate(k(1:Imax,1:iEfmax))
    open (unit=UN,file=DFILE,status='unknown',iostat=ierr,action='read', form='unformatted')
    if (ierr.ne.0) then
       write (STDERR,*) 'Error opening file', DFILE, '; ierr=', ierr
       stop 1
    end if
   do ief=IEfmin, iEfmax
   do ix=1,Nj
       read(UN) fn(ix,ief), k(ix,ief)
   end do
   end do
   write(*,*) "i am here 2, ierr=",ierr, fn(Nj,1),k(Nj,1)
   close(UN)

  allocate(NxatET(1:Nj))
      NxatET=0.
   do ief=IEfmin, iEfmax
         Ef=Emin+deltaE*ief
         if(kbT.ne.0) then
            f_F=1./(1+exp((Ef-E_c)/kbT))
         else
            f_F=1
            if(Ef.eq.E_c) f_F=0.5
            if(Ef.gt.E_c) f_F=0
         end if
         f_FF=0.5*f_F/PI/(E00*k(Imax,ief))
         do ix=1,Nj
            s=f_FF*Fn(ix,ief)
            NxatET(ix)=NxatET(ix)+s
            if (ief.eq.iefmax.or.ief.eq.iefmin) NxatET(ix)=NxatET(ix)-0.5*s
         end do
   end do
      do ix=1,Nj
         Nx1(ix)=(NxatET(ix)+NxatET(Nj-ix+1))*deltaE
      end do
  deallocate(k)
  deallocate(NxatET)
  deallocate(Fn)

end subroutine density
