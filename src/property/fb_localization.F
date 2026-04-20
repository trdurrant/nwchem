      subroutine fb_localization(rtdb,geom,basis,
     N     nbf,nmo,nocc,nvir,
     L     ltyp,
     G     g_movecs)
      implicit none
#include "errquit.fh"
#include "mafdecls.fh"
#include "global.fh"
#include "msgids.fh"
#include "geom.fh"
#include "rtdb.fh"
#include "bas.fh"
#include "util.fh"
#include "stdio.fh"
      integer rtdb,geom,basis,
     N     nbf,nmo,nocc,nvir
      character*3 ltyp
      integer g_movecs
c
      integer g_uc(4),g_smat,g_sc
      integer nloc
      integer l_sc, k_sc, l_c, k_c
      integer l_iloc, k_iloc
      integer l_dip(3), k_dip(3)
      integer i,i_in
      character*(15) pname
      double precision sumd2,dx,dy,dz,rtmp
      integer ga_create_atom_blocked
      external ga_create_atom_blocked
      logical debug
      debug=.false.
c      
c     =================
c       Boys localization
c       =================
      pname = 'fb_localization'

        do i = 1,4
          if (.not. ga_create(MT_DBL, nbf, nbf, 'uc',
     $       nbf, 0, g_uc(i))) call errquit(pname//': uc'
     &       ,i,GA_ERR)
          call ga_zero(g_uc(i))
         end do


c        dipole moment AO matrices -> uc(1) - uc(3)
         call int_dip_ga(basis, basis, g_uc(1), g_uc(2), g_uc(3))


c        calculate dipole-AO times C (mo-coeffs), store in uc(i)
         if (.not. ga_create(MT_DBL, nbf, nmo, 'sc',
     $        nbf, 0, g_sc)) call errquit(pname//': sc',0, GA_ERR)
         do i = 1, 3
            call ga_dgemm('n', 'n', nbf, nmo, nbf, 
     $           1.0d0, g_uc(i), g_movecs, 0.0d0, g_sc)
            call ga_copy_patch('n',g_sc,1,nbf,1,nmo,g_uc(i),1,nbf,1,nmo)
         end do

c       AO Overlap Matrix S:

        g_smat  = ga_create_atom_blocked(geom, basis, 'loc:smat')
        call ga_zero(g_smat)
        call int_1e_ga(basis, basis, g_smat, 'overlap', .false.)

c       store S C in array uc(4)
        call ga_dgemm('n', 'n', nbf, nmo, nbf, 
     $     1.0d0, g_smat, g_movecs, 0.0d0, g_uc(4))

c     
         if (.not. ma_push_get(mt_dbl, 8*nbf, 'sc', l_sc, k_sc))
     $        call errquit(pname//': ma for sc', 0, MA_ERR)
         if (.not. ma_push_get(mt_dbl, 2*nbf, 'c', l_c, k_c))
     $     call errquit(pname//': ma for c', 0, MA_ERR)

         if (.not. ma_push_get(mt_int, nmo, 'iloc', l_iloc,
     &     k_iloc)) call errquit(pname//': loc:iloc', 1, MA_ERR)

         if (debug) write(luout,*) 'MA c, sc complete'

c        localize core and occupied orbitals:
 
         if (ltyp.eq.'occ') then
            do i = 1, nocc
               int_mb(k_iloc - 1 + i) = i
c     iloc(i) = i
            enddo
            nloc = nocc
         else if (ltyp.eq.'vir') then

            do i = nocc+1, nmo
               int_mb(k_iloc - 1 - nocc + i) = i
c              iloc(i-nocc) = i
            end do
            nloc = nmo - nocc
         endif

c        calculate orbital centroids and determine B2 = the sum of 
c        square distances of the centroids from the coordinate origin.
c        The Boys localization maximizes B2. 
c        we can use g_sc as a temp array as it is not further used

         if (.not. ga_destroy(g_sc)) call errquit(
     &      pname//': error destroying g_sc',0, GA_ERR)
         if (.not. ga_create(MT_DBL, nmo, nmo, 'sc',
     $      nbf, 0, g_sc)) call errquit(pname//': sc',0, GA_ERR)

         do i = 1,3
           if (.not. ma_push_get(mt_dbl, nmo, 'sc', l_dip(i), k_dip(i)))
     $        call errquit(pname//': ma for dip', i, MA_ERR)
         end do

         do i = 1,3
           call ga_zero(g_sc)
           call ga_dgemm('t','n',nmo,nmo,nbf, 1.0d0, g_movecs,
     &       g_uc(i), 0.0d0, g_sc)
           call ga_get_diagonal(g_sc, dbl_mb(k_dip(i)))
         end do


         do i = 3,1,-1
           if (.not. ma_pop_stack(l_dip(i))) call errquit(
     &        pname//': error MA pop dip',i, MA_ERR)
         end do

c        jochen: comment:
c        the Boys localization routine was already available
c        in nwchem
         call localizeFB(basis, dbl_mb(k_c), dbl_mb(k_sc), 
     $        nloc, int_mb(k_iloc), nbf, nmo, g_movecs, g_uc)

c        calculate orbital centroids again and print information

         do i = 1,3
           if (.not. ma_push_get(mt_dbl, nmo, 'sc', l_dip(i), k_dip(i)))
     $        call errquit(pname//': ma for dip', i, MA_ERR)
         end do

         do i = 1,3
           call ga_zero(g_sc)
           call ga_dgemm('t','n',nmo,nmo,nbf, 1.0d0, g_movecs,
     &       g_uc(i), 0.0d0, g_sc)
           call ga_get_diagonal(g_sc, dbl_mb(k_dip(i)))
         end do

         if (ga_nodeid().eq.0) then
           write (luout,'(/t6,a,t15,a,t27,a,t39,a,t49,a)')
     &        '#','<x>','<y>','<z>','<r>**2'
           write (luout,'(1x,53(''-''))')
         end if
         sumd2 = 0.0d0
         do i_in = 1,nloc
            i=int_mb(k_iloc + i_in -1)
           dx = dbl_mb(k_dip(1) + i-1)
           dy = dbl_mb(k_dip(2) + i-1)
           dz = dbl_mb(k_dip(3) + i-1)
           rtmp = dx**2 + dy**2 + dz**2
           sumd2 = sumd2 + rtmp
           if (ga_nodeid().eq.0) then
             write (luout,'(1x,i5,4f12.4)') i,dx,dy,dz,rtmp
           end if
         end do
c         if (ga_nodeid().eq.0) then
c           write (luout,'(1x,a,2f12.4)') 'B2 after localization, av:',
c     &        sumd2, sumd2/float(nloc)
c         end if

           if (.not. ma_chop_stack(l_dip(1))) call errquit(
     &        pname//': error MA pop dip',i, MA_ERR)

c        clean up  temp arrays:

         if (.not. ga_destroy(g_sc)) call errquit(
     &      pname//': error destroying g_sc',0, GA_ERR)

         if (.not. ma_chop_stack(l_sc)) call errquit(
     &      pname//': error MA pop sc',0, MA_ERR)

         do i = 1, 4
            if (.not. ga_destroy(g_uc(i)))
     &        call errquit(pname//': error destroying uc',i, GA_ERR)
         end do

c        smat not needed anymore
         if (.not. ga_destroy(g_smat)) call errquit(
     &     pname//': error destroying g_smat',0, GA_ERR)
         return
         end
