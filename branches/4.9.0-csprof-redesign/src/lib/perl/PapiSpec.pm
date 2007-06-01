## $Id$
## * BeginRiceCopyright *****************************************************
## 
## Copyright ((c)) 2002, Rice University 
## All rights reserved.
## 
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are
## met:
## 
## * Redistributions of source code must retain the above copyright
##   notice, this list of conditions and the following disclaimer.
## 
## * Redistributions in binary form must reproduce the above copyright
##   notice, this list of conditions and the following disclaimer in the
##   documentation and/or other materials provided with the distribution.
## 
## * Neither the name of Rice University (RICE) nor the names of its
##   contributors may be used to endorse or promote products derived from
##   this software without specific prior written permission.
## 
## This software is provided by RICE and contributors "as is" and any
## express or implied warranties, including, but not limited to, the
## implied warranties of merchantability and fitness for a particular
## purpose are disclaimed. In no event shall RICE or contributors be
## liable for any direct, indirect, incidental, special, exemplary, or
## consequential damages (including, but not limited to, procurement of
## substitute goods or services; loss of use, data, or profits; or
## business interruption) however caused and on any theory of liability,
## whether in contract, strict liability, or tort (including negligence
## or otherwise) arising in any way out of the use of this software, even
## if advised of the possibility of such damage. 
## 
## ******************************************************* EndRiceCopyright *

# ----------------------------------------------------------------------
# $Id$
# $Author$
# $Date$
# $Revision$
# $Log$
# Revision 1.1  2003/04/21 21:55:04  slindahl
# Initial revision
#
# Revision 1.4  2002/07/17  18:08:52  eraxxon
# Update copyright notices and RCS strings.
#
# Revision 1.3  2002/03/26 13:43:01  eraxxon
# Fix error in RE for file names; misc. updates.
#
# Revision 1.1  2000/02/23  15:50:30  dsystem
# Initial revision
#
# Revision 1.1  2000/02/04 15:24:19  xyuan
# Initial revision
#
# ----------------------------------------------------------------------

# ----------------------------------------------------------------------
#
# PapiSpec.pm
#   PAPI specification module which defines a PAPI-name-to-PAPI-number
#   hash table
#
# ----------------------------------------------------------------------

package PapiSpec;

use strict;

%PapiSpec::symbolValueHashTable =
    ("PAPI_L1_DCM"  => 0x80000000,
     "PAPI_L1_ICM"  => 0x80000001,
     "PAPI_L2_DCM"  => 0x80000002,
     "PAPI_L2_ICM"  => 0x80000003,
     "PAPI_L3_DCM"  => 0x80000004,
     "PAPI_L3_ICM"  => 0x80000005,
     "PAPI_L1_TCM"  => 0x80000006,
     "PAPI_L2_TCM"  => 0x80000007,
     "PAPI_L3_TCM"  => 0x80000008,
     "PAPI_CA_SNP"  => 0x80000009,
     "PAPI_CA_SHR"  => 0x8000000A,
     "PAPI_CA_CLN"  => 0x8000000B,
     "PAPI_CA_INV"  => 0x8000000C,
     "PAPI_CA_ITV"  => 0x8000000D,
     "PAPI_BRU_IDL" => 0x80000010,
     "PAPI_FXU_IDL" => 0x80000011,
     "PAPI_FPU_IDL" => 0x80000012,
     "PAPI_LSU_IDL" => 0x80000013,
     "PAPI_TLB_DM"  => 0x80000014,
     "PAPI_TLB_IM"  => 0x80000015,
     "PAPI_TLB_TL"  => 0x80000016,
     "PAPI_L1_LDM"  => 0x80000017,
     "PAPI_L1_STM"  => 0x80000018,
     "PAPI_L2_LDM"  => 0x80000019,
     "PAPI_L2_STM"  => 0x8000001A,
     "PAPI_TLB_SD"  => 0x8000001E,
     "PAPI_CSR_FAL" => 0x8000001F,
     "PAPI_CSR_SUC" => 0x80000020,
     "PAPI_CSR_TOT" => 0x80000021,
     "PAPI_MEM_SCY" => 0x80000022,
     "PAPI_MEM_RCY" => 0x80000023,
     "PAPI_MEM_WCY" => 0x80000024,
     "PAPI_STL_CYC" => 0x80000025,
     "PAPI_FUL_ICY" => 0x80000026,
     "PAPI_STL_CCY" => 0x80000027,
     "PAPI_FUL_CCY" => 0x80000028,
     "PAPI_BR_UCN"  => 0x8000002A,
     "PAPI_BR_CN"   => 0x8000002B,
     "PAPI_BR_TKN"  => 0x8000002C,
     "PAPI_BR_NTK"  => 0x8000002D,
     "PAPI_BR_MSP"  => 0x8000002E,
     "PAPI_BR_PRC"  => 0x8000002F,
     "PAPI_FMA_INS" => 0x80000030,
     "PAPI_TOT_IIS" => 0x80000031,
     "PAPI_TOT_INS" => 0x80000032,
     "PAPI_INT_INS" => 0x80000033,
     "PAPI_FP_INS"  => 0x80000034,
     "PAPI_LD_INS"  => 0x80000035,
     "PAPI_SR_INS"  => 0x80000036,
     "PAPI_BR_INS"  => 0x80000037,
     "PAPI_VEC_INS" => 0x80000038,
     "PAPI_FLOPS"   => 0x80000039,
     "PAPI_NOOP"    => 0x8000003A,
     "PAPI_FP_STAL" => 0x8000003B,
     "PAPI_TOT_CYC" => 0x8000003C,
     "PAPI_IPS"     => 0x8000003D,
     "PAPI_LST_INS" => 0x8000003E,
     "PAPI_SYC_INS" => 0x8000003F,
     );

1;
