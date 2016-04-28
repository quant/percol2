  subroutine trans(E,V,D,Imax,T,a,b,k)
  use params
  implicit none
  integer :: i,j,m,Nc,in
  integer, intent(in) :: Imax! number of points along along x
  real(WP) :: s,r,c
  real(WP), intent(in) :: E
  real(WP), intent(out) :: T
  real(WP), intent(out) :: k(1:Imax)
  real(WP), intent(in) :: V(1:Imax), D(1:Imax)
  complex(WP) :: cex(1:Imax)
  real(WP) :: ex(1:Imax)
  complex(WP), intent(out):: a(1:Imax),b(1:Imax)
  complex(WP):: cc1,cc2,p
  double(WP):: c
!--------------------start------------
if(E.le.V(1)) then
   T=0.
   return
end if
        do 1 i = 1,Imax
           c = (e-v(i))/E00
            if (c>0) then
                k(i) = cmplx(sqrt( c ), 0);
            else
                k(i) = cmplx(0, sqrt(-c));
            end if
        end do
  complex(WP):: Im,up,dn,K12,KPa,KMa,KPb,KMb
  complex(WP):: ss, com,com1,p11,p12,p21,p22 
  complex(WP):: s11(0:Imax),s21(0:Imax),ss, com,com1,p11,p12,p21,p22 
    do i=0,Imax-1
       Im = cmplx(0.,1.)
       up = 0.5*cexp(Im*k(i)*d(i))
       dn = 0.5*cexp(Im*k(i)*d(i))
       K12= k(i)/k(i+1)
       KPa= (1.+K12)*up
       KMa= (1.-K12)*dn
       KPb= (1.+K12)*dn
       KMb=  (1.-K12)*up
       ss=1./KPb
       s21(i)=-KMb*ss
       s22(i)=ss
       s11(i)=KPa-KMa*KMb*ss
       s12(i)=KMa*ss
    end do
        p11=s11(0)
        p12=s12(0)
        p21=s21(0)
        p22=s22(0)
        Si_11(0)=p11
        Si_21(0)=p21
        Si_22(0)=p22
        Si_12(0)=p12
        do i=1,Imax-1
            com=1./(1.-p12*s21[i])
            com1=p11*com
            p11=s11(i)*com1
            p21=p21+p22*s21(i)*com1;
            p22=p22*(1.+s21(i)*com*p12)*s22(i);
            p12=s12(i)+s11(i)*com*p12*s22(i);
            Si_11(i)=p11;
            Si_21(i)=p21;
            Si_22(i)=p22;
             Si_12(i)=p12;
        end do
        if(E.gt.V(0).and.E.lt.V(Imax)) then
            a(0)=1.;
            b(0)=p11;
            a(N+1)=p21;
            b(N+1)=0.;
            do i=1, Imax-1
                b(i)=(p11-Si_21(i-1))/Si_22(i-1)
                a(i) =Si_11(i-1)+Si_12(i-1)*b(i)
            end do
            do i=1,Imax
                b(i)=b(i)/a(0)
                a(i)=a(i)/a(0)
            end do  
        else
            a(0)=0
            b(0)=p22
            a(Imax)=p12
            b(Imax)=1.
            do i=1,Imax
                b(i)=b(0)/Si_22(i-1)
                a[i]=Si_12(i-1)*b(i)
            end do
            bb=b(0)  
            do i=1,Imax
                b(i)=b(i)/bb
                a(i)=a(i)/bb
            end do  
        end if



!-------------------
           a(1) = (0.,0.)
           b(1) = (1.,0.)
           Nc=(Imax+1)/2
           in=1
           if(E.lt.Ecr) then
           a(Nc)=0.5
           b(Nc)=-0.5
           in=Nc
           do i=1,Nc-1
              a(i)=0
              b(i)=0
           end do
           end if
     do i = in,Imax-1       ! было i1
!---------------------------------------------------
!           call sew(i)
!---------------------------------------------------
           r = d(i+1)
           if(k(i+1).ne.0.) c = k(i)/k(i+1)
           cc1 = a(i) + b(i)
           cc2 = c*(a(i)-b(i))
           if(e-v(i)) 102,103,104
102        if(e-v(i+1)) 105,106,107
105        a(i+1) = .5*ex(i+1)*(cc1+cc2)
           b(i+1) = .5/ex(i+1)*(cc1-cc2)
           goto 100
106        a(i+1) = k(i)*(b(i)-a(i))
           b(i+1) = cc1+r*a(i+1)
           goto 100
107        cc2 = cmplx(-aimag(cc2),real(cc2))
           p = .5*cex(i+1)
           a(i+1) = p*(cc1+cc2)
           b(i+1) = conjg(p)*(cc1-cc2)
           goto 100
103        if(e-v(i+1)) 108,109,110
108        p = a(i)/k(i+1)
           a(i+1) = .5*ex(i+1)*(b(i)-p)
           b(i+1) = .5/ex(i+1)*(b(i)+p)
           goto 100
109        a(i+1) = a(i)
           b(i+1) = b(i)+r*a(i)
           goto 100
110        p = cmplx(-aimag(a(i))/k(i+1),real(a(i))/k(i+1))
           a(i+1) = .5*cex(i+1)*(b(i)-p)
           b(i+1) = .5*conjg(cex(i+1))*(b(i)+p)
           goto 100
104        if(e-v(i+1)) 111,112,113
111        cc2 = cmplx(-aimag(cc2),real(cc2))
           a(i+1) = .5*ex(i+1)*(cc1-cc2)
           b(i+1) = .5/ex(i+1)*(cc1+cc2)
           goto 100
112        a(i+1) = cmplx(-k(i)*aimag(a(i)-b(i)),k(i)*real(a(i)-b(i)))
           b(i+1) = cc1+r*a(i+1)
           goto 100
113        a(i+1) = .5*cex(i+1)*(cc1+cc2)
           b(i+1) = .5*conjg(cex(i+1))*(cc1-cc2)
100    end do
        T = abs(b(Imax))
        T = T*T
        if(k(1).ne.0.and.k(1).ne.k(Imax)) T=k(Imax)/k(1)*T
        if(T.ne.0.) then
        T = 1./T
        end if
end subroutine trans
!
subroutine wfunx(E,V,D,Imax,a,b,k,fn)
  use params
  implicit none
  integer :: i,ik,n,n1,n2
  integer, intent(in) :: Imax! number of points along x
  real(WP) :: h,xr,x,s,ds,ex,dex,dex1,f
  real(WP), intent(in) :: E
  real(WP), intent(in) :: k(1:Imax)
  real(WP), intent(in) :: V(1:Imax), D(1:Imax)
  real(WP), intent(out) :: fn(1:Nj)
  complex(WP), intent(inout):: a(1:Imax),b(1:Imax)
  complex(WP):: aa,bb,ff,cex,cdex,cdex1
      DO i=1,Imax
         A(i)=A(i)/B(Imax)
         B(I)=B(i)/B(Imax)
      END DO
      h=(xmax-xmin)/(nj-1)
              xr = 0.
              DO i =1,Imax
                 xr = xr+d(i)
              end do
        n1=1
!        xr=0
        xr=xmin
        DO i = 1, Imax
           AA = A(i)
           BB = B(i)
           xr = xr+d(i)
           if(xmax.ge.xr) n2 = (xr-xmin)/h +1
           IF(i.EQ.Imax) n2=Nj
           x = xmin+h*(n1-1)-xr
           s = k(i)*x
           ds = k(i)*h
         IF(e-v(i)) 10,11,12
10         ex = exp(-s)
           dex = exp(-ds)
           dex1= 1./dex
           aa = aa*ex
           bb = bb/ex
         DO n = n1,n2
            aa = aa*dex
            bb = bb*dex1
            ff = aa+bb
            f = ff*conjg(ff)
        fn(n) = f
        end do
        goto 60
11         DO n = n1,n2
            x = x+h
            ff = aa*x+bb
            f = ff*conjg(ff)
        fn(n) = f
        end do
        goto 60
12        cex = cexp(cmplx(0.,s))
          cdex = cexp(cmplx(0.,ds))
          cdex1 = conjg(cdex)
          aa = aa*cex
          bb = bb*conjg(cex)
         DO n = n1,n2
            aa = aa*cdex
            bb = bb*cdex1
            ff = aa+bb
            f = ff*conjg(ff)
        fn(n) = f
        end do
60      n1 = n2+1
    END DO
end subroutine wfunx
