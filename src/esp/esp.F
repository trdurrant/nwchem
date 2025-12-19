      logical function esp(irtdb0)
c
c $Id$
c
      implicit none
c
#include "esp_params.fh"
#include "esp_common.fh"
#include "global.fh"
#include "mafdecls.fh"
#include "rtdb.fh"
#include "inp.fh"
#include "util.fh"
#include "stdio.fh"
c
      integer irtdb0
c
      call util_print_push
      call util_print_rtdb_load(irtdb,'esp')
c
c     me : number current node
c     np : number of nodes
c
      me=ga_nodeid()
      np=ga_nnodes()
      irtdb=irtdb0
c      lfnout=6
      lfnout=luout
      lfngrd=24
      lfner=25
      lfnplt=26
      lfnxyz=27
      lfnchg=28
c
c     read input from rtdb
c
      call esp_rtdbin
c
      if(util_print('output',print_medium)) then
      if(me.eq.0) then
      if(util_print('information',print_default)) then
      write(lfnout,*)
      write(lfnout,*)
      call util_print_centered(6,
     + 'NWChem Electrostatic Potential Fit Module',40,.true.)
      write(lfnout,*)
      write(lfnout,*)
      endif
      call ecce_print_module_entry('nwesp')
      endif
      endif
      
      write(lfnout,*) "newgrd", newgrd
      write(lfnout,*) "newpot", newpot
      write(lfnout,*) "nofit", nofit
c
c     allocate memory
c
      call esp_malloc
c
c     setup the esp grid
c
      write(lfnout,*) "Call to esp_grid"
      call esp_grid(dbl_mb(ixptr),dbl_mb(iqptr),dbl_mb(igptr),
     + dbl_mb(ivptr))
c
c     write input variables to output     
c
      write(lfnout,*) "Call to esp_wrtinp"
      call esp_wrtinp(dbl_mb(iqptr))
c
c     calculate the electrostatic potential on the grid
c
        if(newpot.ne.0.or.denmat.ne.0) then 
        write(lfnout,*) "New pot requested"
         if(me.eq.0) write(6,*) 'FASTESP ',fastesp
         if(fastesp) then
            call esp_fastesp(dbl_mb(ieptr), !ene
     +           dbl_mb(ioptr), !occ 
     +           dbl_mb(idptr), !dns
     +           dbl_mb(ixptr), !x
     +           dbl_mb(iqptr), !q
     +           dbl_mb(igptr), !grid
     +           dbl_mb(ivptr)) !val
         else
            call esp_esp(dbl_mb(ieptr), !ene
     +           dbl_mb(ioptr), !occ 
     +           dbl_mb(idptr), !dns
     +           dbl_mb(ixptr), !x
     +           dbl_mb(iqptr), !q
     +           dbl_mb(igptr), !grid
     +           dbl_mb(ivptr)) !val
         endif
      end if ! newgrd, denmat

      if(nofit.eq.0) then
         write(lfnout,*) "Starting charge fitting"
c
c        do the fitting
c
         call esp_fit(dbl_mb(igptr),dbl_mb(ivptr),dbl_mb(ixptr),
     +    dbl_mb(iqptr),dbl_mb(iaptr),dbl_mb(ibptr),dbl_mb(icptr),
     +    dbl_mb(iakptr),dbl_mb(iqkptr),int_mb(inptr),dbl_mb(iqfptr),
     +    int_mb(ilptr),byte_mb(itptr))
c
c        calculate the electrical moments
c
         call esp_elemom(dbl_mb(ixptr),dbl_mb(iqfptr))
c
c        calculate rms deviation
c
         call esp_rms(dbl_mb(ixptr),dbl_mb(iqfptr),dbl_mb(igptr),
     +    dbl_mb(ivptr))
c
c        write results
c
         call esp_out(byte_mb(itptr),dbl_mb(ixptr),dbl_mb(iqfptr),
     +    dbl_mb(igptr),dbl_mb(ivptr))
c
c        electrostatic potential from partial charges on full grid
c
         call esp_plt(dbl_mb(ixptr),dbl_mb(iqfptr),dbl_mb(igptr),
     +    byte_mb(itptr))
      end if ! nofit
c
c     deallocate memory
c
      call esp_dalloc
c
      call util_print_pop
c
      esp=.true.
      return
      end
