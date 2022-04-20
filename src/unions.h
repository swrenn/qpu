// Copyright 2022 Samuel Wrenn
//
// This file is part of QPU.
//
// QPU is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// QPU is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// QPU. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "types.h"

enum {
  V3D_IDENT0  = 0xc00000,
  V3D_IDENT1  = 0xc00004,
  V3D_IDENT2  = 0xc00008,
  V3D_SCRATCH = 0xc00010,
  V3D_L2CACTL = 0xc00020,
  V3D_SLCACTL = 0xc00024,
  V3D_INTCTL  = 0xc00030,
  V3D_INTENA  = 0xc00034,
  V3D_INTDIS  = 0xc00038,
  V3D_CT0CS   = 0xc00100,
  V3D_CT1CS   = 0xc00104,
  V3D_CT0EA   = 0xc00108,
  V3D_CT1EA   = 0xc0010c,
  V3D_CT0CA   = 0xc00110,
  V3D_CT1CA   = 0xc00114,
  V3D_CT00RA0 = 0xc00118,
  V3D_CT01RA0 = 0xc0011c,
  V3D_CT0LC   = 0xc00120,
  V3D_CT1LC   = 0xc00124,
  V3D_CT0PC   = 0xc00128,
  V3D_CT1PC   = 0xc0012c,
  V3D_PCS     = 0xc00130,
  V3D_BFC     = 0xc00134,
  V3D_RFC     = 0xc00138,
  V3D_BPCA    = 0xc00300,
  V3D_BPCS    = 0xc00304,
  V3D_BPOA    = 0xc00308,
  V3D_BPOS    = 0xc0030c,
  V3D_BXCF    = 0xc00310,
  V3D_SQRSV0  = 0xc00410,
  V3D_SQRSV1  = 0xc00414,
  V3D_SQCNTL  = 0xc00418,
  V3D_SRQPC   = 0xc00430,
  V3D_SRQUA   = 0xc00434,
  V3D_SRQUL   = 0xc00438,
  V3D_SRQCS   = 0xc0043c,
  V3D_VPACNTL = 0xc00500,
  V3D_VPMBASE = 0xc00504,
  V3D_PCTRC   = 0xc00670,
  V3D_PCTRE   = 0xc00674,
  V3D_PCTR0   = 0xc00680,
  V3D_PCTRS0  = 0xc00684,
  V3D_PCTR1   = 0xc00688,
  V3D_PCTRS1  = 0xc0068c,
  V3D_PCTR2   = 0xc00690,
  V3D_PCTRS2  = 0xc00694,
  V3D_PCTR3   = 0xc00698,
  V3D_PCTRS3  = 0xc0069c,
  V3D_PCTR4   = 0xc006a0,
  V3D_PCTRS4  = 0xc006a4,
  V3D_PCTR5   = 0xc006a8,
  V3D_PCTRS5  = 0xc006ac,
  V3D_PCTR6   = 0xc006b0,
  V3D_PCTRS6  = 0xc006b4,
  V3D_PCTR7   = 0xc006b8,
  V3D_PCTRS7  = 0xc006bc,
  V3D_PCTR8   = 0xc006c0,
  V3D_PCTRS8  = 0xc006c4,
  V3D_PCTR9   = 0xc006c8,
  V3D_PCTRS9  = 0xc006cc,
  V3D_PCTR10  = 0xc006d0,
  V3D_PCTRS10 = 0xc006d4,
  V3D_PCTR11  = 0xc006d8,
  V3D_PCTRS11 = 0xc006dc,
  V3D_PCTR12  = 0xc006e0,
  V3D_PCTRS12 = 0xc006e4,
  V3D_PCTR13  = 0xc006e8,
  V3D_PCTRS13 = 0xc006ec,
  V3D_PCTR14  = 0xc006f0,
  V3D_PCTRS14 = 0xc006f4,
  V3D_PCTR15  = 0xc006f8,
  V3D_PCTRS15 = 0xc006fc,
  V3D_DBQITE  = 0xc00e2c,
  V3D_DBQITC  = 0xc00e30,
  V3D_DBGE    = 0xc00f00,
  V3D_FDBGO   = 0xc00f04,
  V3D_FDBGB   = 0xc00f08,
  V3D_FDBGR   = 0xc00f0c,
  V3D_FDBGS   = 0xc00f10,
  V3D_ERRSTAT = 0xc00f20,
};

typedef union IDENT0 {
  u32 w;
  struct {
    u32 idstr0 : 8;
    u32 idstr1 : 8;
    u32 idstr2 : 8;
    u32 tver   : 8;
  } f;
} IDENT0;

typedef union IDENT1 {
  u32 w;
  struct {
    u32 rev   : 4;
    u32 nslc  : 4;
    u32 qups  : 4;
    u32 tups  : 4;
    u32 nsem  : 8;
    u32 hdrt  : 4;
    u32 vpmsz : 4;
  } f;
} IDENT1;

typedef union IDENT2 {
  u32 w;
  struct {
    u32 vrisz    : 4;
    u32 tlbsz    : 4;
    u32 tlbdb    : 4;
    u32 reserved : 20;
  } f;
} IDENT2;

typedef union L2CACTL {
  u32 w;
  struct {
    u32 l2cena   : 1;
    u32 l2cdis   : 1;
    u32 l2cclr   : 1;
    u32 reserved : 29;
  } f;
} L2CACTL;

typedef union SLCACTL {
  u32 w;
  struct {
    u32 iccs0_to_iccs3   : 4;
    u32 reserved0        : 4;
    u32 uccs0_to_uccs3   : 4;
    u32 reserved1        : 4;
    u32 t0ccs0_to_t0ccs3 : 4;
    u32 reserved2        : 4;
    u32 t1ccs0_to_t1ccs3 : 4;
    u32 reserved3        : 4;
  } f;
} SLCACTL;

typedef union INTCTL {
  u32 w;
  struct {
    u32 int_frdone   : 1;
    u32 int_fldone   : 1;
    u32 int_outomem  : 1;
    u32 int_spilluse : 1;
    u32 reserved     : 28;
  } f;
} INTCTL;

typedef union INTENA {
  u32 w;
  struct {
    u32 ei_frdone   : 1;
    u32 ei_fldone   : 1;
    u32 ei_outomem  : 1;
    u32 ei_spilluse : 1;
    u32 reserved    : 28;
  } f;
} INTENA;

typedef union INTDIS {
  u32 w;
  struct {
    u32 di_frdone   : 1;
    u32 di_fldone   : 1;
    u32 di_outomem  : 1;
    u32 di_spilluse : 1;
    u32 reserved    : 28;
  } f;
} INTDIS;

typedef union CTnCS {
  u32 w;
  struct {
    u32 ctmode    : 1;
    u32 reserved0 : 2;
    u32 cterr     : 1;
    u32 ctsubs    : 1;
    u32 ctrun     : 1;
    u32 reserved1 : 2;
    u32 ctrtsd    : 2;
    u32 reserved2 : 2;
    u32 ctsema    : 3;
    u32 ctrsta    : 1;
    u32 reserved3 : 16;
  } f;
} CTnCS;

typedef union CTnLC {
  u32 w;
  struct {
    u32 ctlslcs : 16;
    u32 ctllcm  : 16;
  } f;
} CTnLC;

typedef union PCS {
  u32 w;
  struct {
    u32 bmactive  : 1;
    u32 bmbusy    : 1;
    u32 rmactive  : 1;
    u32 rmbusy    : 1;
    u32 reserved0 : 4;
    u32 bmoom     : 1;
    u32 reserved1 : 23;
  } f;
} PCS;

typedef union BFC {
  u32 w;
  struct {
    u32 bmfct    : 8;
    u32 reserved : 24;
  } f;
} BFC;

typedef union RFC {
  u32 w;
  struct {
    u32 rmfct    : 8;
    u32 reserved : 24;
  } f;
} RFC;

typedef union BXCF {
  u32 w;
  struct {
    u32 fwddisa   : 1;
    u32 clipdisa  : 1;
    u32 reserved1 : 30;
  } f;
} BXCF;

typedef union SQRSV0 {
  u32 w;
  struct {
    u32 qpursv0 : 4;
    u32 qpursv1 : 4;
    u32 qpursv2 : 4;
    u32 qpursv3 : 4;
    u32 qpursv4 : 4;
    u32 qpursv5 : 4;
    u32 qpursv6 : 4;
    u32 qpursv7 : 4;
  } f;
} SQRSV0;

typedef union SQRSV1 {
  u32 w;
  struct {
    u32 qpursv8  : 4;
    u32 qpursv9  : 4;
    u32 qpursv10 : 4;
    u32 qpursv11 : 4;
    u32 qpursv12 : 4;
    u32 qpursv13 : 4;
    u32 qpursv14 : 4;
    u32 qpursv15 : 4;
  } f;
} SQRSV1;

typedef union SQCNTL {
  u32 w;
  struct {
    u32 vsrbl    : 2;
    u32 csrbl    : 2;
    u32 reserved : 28;
  } f;
} SQCNTL;

typedef union SRQUL {
  u32 w;
  struct {
    u32 qpurqul  : 12;
    u32 reserved : 20;
  } f;
} SRQUL;

typedef union SRQCS {
  u32 w;
  struct {
    u32 qpurql    : 6;
    u32 reserved1 : 1;
    u32 qpurqerr  : 1;
    u32 qpurqcm   : 8;
    u32 qpurqcc   : 8;
    u32 reserved2 : 8;
  } f;
} SRQCS;

typedef union VPACNTL {
  u32 w;
  struct {
    u32 vparalim : 3;
    u32 vpabalim : 3;
    u32 vparato  : 3;
    u32 vpabato  : 3;
    u32 vpalimen : 1;
    u32 vpatoen  : 1;
    u32 reserved : 18;
  } f;
} VPACNTL;

typedef union VPMBASE {
  u32 w;
  struct {
    u32 vpmursv  : 5;
    u32 reserved : 27;
  } f;
} VPMBASE;

typedef union PCTRC {
  u32 w;
  struct {
    u32 ctclr0   : 1;
    u32 ctclr1   : 1;
    u32 ctclr2   : 1;
    u32 ctclr3   : 1;
    u32 ctclr4   : 1;
    u32 ctclr5   : 1;
    u32 ctclr6   : 1;
    u32 ctclr7   : 1;
    u32 ctclr8   : 1;
    u32 ctclr9   : 1;
    u32 ctclr10  : 1;
    u32 ctclr11  : 1;
    u32 ctclr12  : 1;
    u32 ctclr13  : 1;
    u32 ctclr14  : 1;
    u32 ctclr15  : 1;
    u32 reserved : 16;
  } f;
} PCTRC;

typedef union PCTRE {
  u32 w;
  struct {
    u32 cten0    : 1;
    u32 cten1    : 1;
    u32 cten2    : 1;
    u32 cten3    : 1;
    u32 cten4    : 1;
    u32 cten5    : 1;
    u32 cten6    : 1;
    u32 cten7    : 1;
    u32 cten8    : 1;
    u32 cten9    : 1;
    u32 cten10   : 1;
    u32 cten11   : 1;
    u32 cten12   : 1;
    u32 cten13   : 1;
    u32 cten14   : 1;
    u32 cten15   : 1;
    u32 reserved : 15;
    u32 enable   : 1;
  } f;
} PCTRE;

typedef union PCTRSn {
  u32 w;
  struct {
    u32 pctrs    : 5;
    u32 reserved : 27;
  } f;
} PCTRSn;

typedef union DBQITE {
  u32 w;
  struct {
    u32 ie_qpu0  : 1;
    u32 ie_qpu1  : 1;
    u32 ie_qpu2  : 1;
    u32 ie_qpu3  : 1;
    u32 ie_qpu4  : 1;
    u32 ie_qpu5  : 1;
    u32 ie_qpu6  : 1;
    u32 ie_qpu7  : 1;
    u32 ie_qpu8  : 1;
    u32 ie_qpu9  : 1;
    u32 ie_qpu10 : 1;
    u32 ie_qpu11 : 1;
    u32 ie_qpu12 : 1;
    u32 ie_qpu13 : 1;
    u32 ie_qpu14 : 1;
    u32 ie_qpu15 : 1;
    u32 reserved : 16;
  } f;
} DBQITE;

typedef union DBQITC {
  u32 w;
  struct {
    u32 ic_qpu0  : 1;
    u32 ic_qpu1  : 1;
    u32 ic_qpu2  : 1;
    u32 ic_qpu3  : 1;
    u32 ic_qpu4  : 1;
    u32 ic_qpu5  : 1;
    u32 ic_qpu6  : 1;
    u32 ic_qpu7  : 1;
    u32 ic_qpu8  : 1;
    u32 ic_qpu9  : 1;
    u32 ic_qpu10 : 1;
    u32 ic_qpu11 : 1;
    u32 ic_qpu12 : 1;
    u32 ic_qpu13 : 1;
    u32 ic_qpu14 : 1;
    u32 ic_qpu15 : 1;
    u32 reserved : 16;
  } f;
} DBQITC;

typedef union DBGE {
  u32 w;
  struct {
    u32 reserved0    : 1;
    u32 vr1_a        : 1;
    u32 vr1_b        : 1;
    u32 reserved1    : 13;
    u32 mulip0       : 1;
    u32 mulip1       : 1;
    u32 mulip2       : 1;
    u32 ipd2_valid   : 1;
    u32 ipd2_fpdused : 1;
    u32 reserved2    : 11;
  } f;
} DBGE;

typedef union FDBGO {
  u32 w;
  struct {
    u32 reserved0        : 1;
    u32 wcoeff_fifo_full : 1;
    u32 xyrelz_fifo_full : 1;
    u32 qbfr_fifo_orun   : 1;
    u32 qbsz_fifo_orun   : 1;
    u32 xyfo_fifo_orun   : 1;
    u32 fixz_orun        : 1;
    u32 xyrelo_fifo_orun : 1;
    u32 reserved1        : 2;
    u32 xyrelw_fifo_orun : 1;
    u32 zcoeff_fifo_full : 1;
    u32 refxy_fifo_orun  : 1;
    u32 deptho_fifo_orun : 1;
    u32 deptho_orun      : 1;
    u32 ezval_fifo_orun  : 1;
    u32 reserved2        : 1;
    u32 ezreq_fifo_orun  : 1;
    u32 reserved3        : 14;
  } f;
} FDBGO;

typedef union FDBGB {
  u32 w;
  struct {
    u32 edges_stall        : 1;
    u32 edges_ready        : 1;
    u32 edges_isctrl       : 1;
    u32 edges_ctrlid       : 3;
    u32 zrwpe_stall        : 1;
    u32 zrwpe_ready        : 1;
    u32 reserved0          : 15;
    u32 ez_data_ready      : 1;
    u32 reserved1          : 1;
    u32 ez_xy_ready        : 1;
    u32 rast_busy          : 1;
    u32 qxyf_fifo_op_ready : 1;
    u32 xyfo_fifo_op_ready : 1;
    u32 reserved2          : 3;
  } f;
} FDBGB;

typedef union FDBGR {
  u32 w;
  struct {
    u32 qxyf_fifo_ready   : 1;
    u32 ezreq_fifo_ready  : 1;
    u32 ezval_fifo_ready  : 1;
    u32 deptho_fifo_ready : 1;
    u32 refxy_fifo_ready  : 1;
    u32 zcoeff_fifo_ready : 1;
    u32 xyrelw_fifo_ready : 1;
    u32 wcoeff_fifo_ready : 1;
    u32 reserved0         : 3;
    u32 xyrelo_fifo_ready : 1;
    u32 reserved1         : 1;
    u32 zo_fifo_ready     : 1;
    u32 xyfo_fifo_ready   : 1;
    u32 reserved2         : 1;
    u32 rast_ready        : 1;
    u32 rast_last         : 1;
    u32 deptho_ready      : 1;
    u32 ezlim_ready       : 1;
    u32 xynrm_ready       : 1;
    u32 xynrm_last        : 1;
    u32 xyrelz_fifo_ready : 1;
    u32 xyrelz_fifo_last  : 1;
    u32 interpz_ready     : 1;
    u32 reserved3         : 2;
    u32 interprw_ready    : 1;
    u32 recipw_ready      : 1;
    u32 reserved4         : 1;
    u32 fixz_ready        : 1;
    u32 reserved5         : 1;
  } f;
} FDBGR;

typedef union FDBGS {
  u32 w;
  struct {
    u32 eztest_ip_qstall     : 1;
    u32 eztest_ip_prstall    : 1;
    u32 eztest_ip_vlfstall   : 1;
    u32 eztest_stall         : 1;
    u32 eztest_vlf_oknovalid : 1;
    u32 eztest_qready        : 1;
    u32 eztest_anyqf         : 1;
    u32 eztest_anyqvalid     : 1;
    u32 qxyf_fifo_op1_valid  : 1;
    u32 qxyf_fifo_op1_last   : 1;
    u32 qxyf_fifo_op1_dummy  : 1;
    u32 qxyf_fifo_op_last    : 1;
    u32 qxyf_fifo_op_valid   : 1;
    u32 ezreq_fifo_op_valid  : 1;
    u32 xynrm_ip_stall       : 1;
    u32 ezlim_ip_stall       : 1;
    u32 deptho_fifo_ip_stall : 1;
    u32 interpz_ip_stall     : 1;
    u32 xyrelz_fifo_ip_stall : 1;
    u32 reserved0            : 3;
    u32 interpw_ip_stall     : 1;
    u32 reserved1            : 2;
    u32 recipw_ip_stall      : 1;
    u32 reserved2            : 2;
    u32 zo_fifo_ip_stall     : 1;
    u32 reserved3            : 3;
  } f;
} FDBGS;

typedef union ERRSTAT {
  u32 w;
  struct {
    u32 vpaeabb  : 1;
    u32 vpaergs  : 1;
    u32 vpaebrgl : 1;
    u32 vpaerrgl : 1;
    u32 vpmewr   : 1;
    u32 vpmerr   : 1;
    u32 vpmerna  : 1;
    u32 vpmewna  : 1;
    u32 vpmefna  : 1;
    u32 vpmeas   : 1;
    u32 vdwe     : 1;
    u32 vcde     : 1;
    u32 vcdi     : 1;
    u32 vcmre    : 1;
    u32 vcmbe    : 1;
    u32 l2care   : 1;
    u32 reserved : 16;
  } f;
} ERRSTAT;
