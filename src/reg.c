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

#include "reg.h"

#include "log.h"
#include "mem.h"
#include "types.h"
#include "unions.h"

#include <assert.h>
#include <bcm_host.h>
#include <stdio.h>

#define D(x) (diff.x = b.x - a.x)
#define C(x) (x < nperfctr ? perfctr[x].desc : "Invalid")

#define R(x)                                                \
  (((x) == 0b0000)   ? "User, Fragment, Vertex, Coordinate" \
   : ((x) == 0b0001) ? "Fragment, Vertex, Coordinate"       \
   : ((x) == 0b0010) ? "User, Vertex, Coordinate"           \
   : ((x) == 0b0011) ? "Vertex, Coordinate"                 \
   : ((x) == 0b0100) ? "User, Fragment, Coordinate"         \
   : ((x) == 0b0101) ? "Fragment, Coordinate"               \
   : ((x) == 0b0110) ? "User, Coordinate"                   \
   : ((x) == 0b0111) ? "Coordinate"                         \
   : ((x) == 0b1000) ? "User, Fragment, Vertex"             \
   : ((x) == 0b1001) ? "Fragment, Vertex"                   \
   : ((x) == 0b1010) ? "User, Vertex"                       \
   : ((x) == 0b1011) ? "Vertex"                             \
   : ((x) == 0b1100) ? "User, Fragment"                     \
   : ((x) == 0b1101) ? "Fragment"                           \
   : ((x) == 0b1110) ? "User"                               \
                     : "None")

#define CHECK_ENABLED()          \
  do {                           \
    if (!reg_gpu_is_enabled()) { \
      NOTICE("(GPU disabled)");  \
      return;                    \
    }                            \
  } while (false)

typedef struct PCTRS {
  PCTRSn c0;
  PCTRSn c1;
  PCTRSn c2;
  PCTRSn c3;
  PCTRSn c4;
  PCTRSn c5;
  PCTRSn c6;
  PCTRSn c7;
  PCTRSn c8;
  PCTRSn c9;
  PCTRSn c10;
  PCTRSn c11;
  PCTRSn c12;
  PCTRSn c13;
  PCTRSn c14;
  PCTRSn c15;
} PCTRS;

typedef struct PCTR {
  u32 c0;
  u32 c1;
  u32 c2;
  u32 c3;
  u32 c4;
  u32 c5;
  u32 c6;
  u32 c7;
  u32 c8;
  u32 c9;
  u32 c10;
  u32 c11;
  u32 c12;
  u32 c13;
  u32 c14;
  u32 c15;
} PCTR;

typedef struct debug {
  ERRSTAT errstat;
  FDBGS fdbgs;
  FDBGR fdbgr;
  FDBGB fdbgb;
  FDBGO fdbgo;
  DBGE dbge;
  DBQITC dbqitc;
  SRQCS srqcs;
  u32 scratch;
} debug;

static const struct {
  u32 id;
  const char *desc;
} perfctr[] = {
  {0, "Valid primitives for all rendered tiles with no rendered pixels"},
  {1, "Valid primitives for all rendered tiles"},
  {2, "Early-Z/Near/Far clipped quads"},
  {3, "Valid quads"},
  {4, "Quads with no pixels passing the stencil test"},
  {5, "Quads with no pixels passing the Z and stencil tests"},
  {6, "Quads with any pixels passing the Z and stencil tests"},
  {7, "Quads with all pixels having zero coverage"},
  {8, "Quads with any pixels having non-zero coverage"},
  {9, "Quads with valid pixels written to color buffer"},
  {10, "Primitives discarded by being outside the viewport"},
  {11, "Primitives that need clipping"},
  {12, "Primitives that are discarded because they are reversed"},
  {13, "Total idle clock cycles for all QPUs"},
  {14, "Total clock cycles for QPUs doing vertex/coordinate shading"},
  {15, "Total clock cycles for QPUs doing fragment shading"},
  {16, "Total clock cycles for QPUs executing valid instructions"},
  {17, "Total clock cycles for QPUs stalled waiting for TMUs"},
  {18, "Total clock cycles for QPUs stalled waiting for Scoreboard"},
  {19, "Total clock cycles for QPUs stalled waiting for Varyings"},
  {20, "Total instruction cache hits for all slices"},
  {21, "Total instruction cache misses for all slices"},
  {22, "Total uniforms cache hits for all slices"},
  {23, "Total uniforms cache misses for all slices"},
  {24, "Total texture quads processed"},
  {25, "Total texture cache misses"},
  {26, "Total clock cycles VDW is stalled waiting for VPM access"},
  {27, "Total clock cycles VCD is stalled waiting for VPM access"},
  {28, "Total level 2 cache hits"},
  {29, "Total level 2 cache misses"},
};

static const u32 nperfctr = sizeof(perfctr) / sizeof(perfctr[0]);

// Globals
static struct {
  struct {
    uaddr addr;
    u32 sz;
  } map;
  struct {
    PCTR before;
    PCTR after;
  } perf;
  struct {
    debug before;
    debug after;
  } debug;
} G;

//
// Write Helpers
//

static void
write_DBQITC(DBQITC u) {
  DBQITC m = {
    .f.ic_qpu0  = u.f.ic_qpu0,
    .f.ic_qpu1  = u.f.ic_qpu1,
    .f.ic_qpu2  = u.f.ic_qpu2,
    .f.ic_qpu3  = u.f.ic_qpu3,
    .f.ic_qpu4  = u.f.ic_qpu4,
    .f.ic_qpu5  = u.f.ic_qpu5,
    .f.ic_qpu6  = u.f.ic_qpu6,
    .f.ic_qpu7  = u.f.ic_qpu7,
    .f.ic_qpu8  = u.f.ic_qpu8,
    .f.ic_qpu9  = u.f.ic_qpu9,
    .f.ic_qpu10 = u.f.ic_qpu10,
    .f.ic_qpu11 = u.f.ic_qpu11,
    .f.ic_qpu12 = u.f.ic_qpu12,
    .f.ic_qpu13 = u.f.ic_qpu13,
    .f.ic_qpu14 = u.f.ic_qpu14,
    .f.ic_qpu15 = u.f.ic_qpu15,
  };
  *(volatile u32 *)(G.map.addr + V3D_DBQITC) = m.w;
}

static void
write_DBQITE(DBQITE u) {
  DBQITE m = {
    .f.ie_qpu0  = u.f.ie_qpu0,
    .f.ie_qpu1  = u.f.ie_qpu1,
    .f.ie_qpu2  = u.f.ie_qpu2,
    .f.ie_qpu3  = u.f.ie_qpu3,
    .f.ie_qpu4  = u.f.ie_qpu4,
    .f.ie_qpu5  = u.f.ie_qpu5,
    .f.ie_qpu6  = u.f.ie_qpu6,
    .f.ie_qpu7  = u.f.ie_qpu7,
    .f.ie_qpu8  = u.f.ie_qpu8,
    .f.ie_qpu9  = u.f.ie_qpu9,
    .f.ie_qpu10 = u.f.ie_qpu10,
    .f.ie_qpu11 = u.f.ie_qpu11,
    .f.ie_qpu12 = u.f.ie_qpu12,
    .f.ie_qpu13 = u.f.ie_qpu13,
    .f.ie_qpu14 = u.f.ie_qpu14,
    .f.ie_qpu15 = u.f.ie_qpu15,
  };
  *(volatile u32 *)(G.map.addr + V3D_DBQITE) = m.w;
}

static void
write_PCTRS15(PCTRSn u) {
  PCTRSn m = {
    .f.pctrs = u.f.pctrs,
  };
  *(volatile u32 *)(G.map.addr + V3D_PCTRS15) = m.w;
}

static void
write_PCTRS14(PCTRSn u) {
  PCTRSn m = {
    .f.pctrs = u.f.pctrs,
  };
  *(volatile u32 *)(G.map.addr + V3D_PCTRS14) = m.w;
}

static void
write_PCTRS13(PCTRSn u) {
  PCTRSn m = {
    .f.pctrs = u.f.pctrs,
  };
  *(volatile u32 *)(G.map.addr + V3D_PCTRS13) = m.w;
}

static void
write_PCTRS12(PCTRSn u) {
  PCTRSn m = {
    .f.pctrs = u.f.pctrs,
  };
  *(volatile u32 *)(G.map.addr + V3D_PCTRS12) = m.w;
}

static void
write_PCTRS11(PCTRSn u) {
  PCTRSn m = {
    .f.pctrs = u.f.pctrs,
  };
  *(volatile u32 *)(G.map.addr + V3D_PCTRS11) = m.w;
}

static void
write_PCTRS10(PCTRSn u) {
  PCTRSn m = {
    .f.pctrs = u.f.pctrs,
  };
  *(volatile u32 *)(G.map.addr + V3D_PCTRS10) = m.w;
}

static void
write_PCTRS9(PCTRSn u) {
  PCTRSn m = {
    .f.pctrs = u.f.pctrs,
  };
  *(volatile u32 *)(G.map.addr + V3D_PCTRS9) = m.w;
}

static void
write_PCTRS8(PCTRSn u) {
  PCTRSn m = {
    .f.pctrs = u.f.pctrs,
  };
  *(volatile u32 *)(G.map.addr + V3D_PCTRS8) = m.w;
}

static void
write_PCTRS7(PCTRSn u) {
  PCTRSn m = {
    .f.pctrs = u.f.pctrs,
  };
  *(volatile u32 *)(G.map.addr + V3D_PCTRS7) = m.w;
}

static void
write_PCTRS6(PCTRSn u) {
  PCTRSn m = {
    .f.pctrs = u.f.pctrs,
  };
  *(volatile u32 *)(G.map.addr + V3D_PCTRS6) = m.w;
}

static void
write_PCTRS5(PCTRSn u) {
  PCTRSn m = {
    .f.pctrs = u.f.pctrs,
  };
  *(volatile u32 *)(G.map.addr + V3D_PCTRS5) = m.w;
}

static void
write_PCTRS4(PCTRSn u) {
  PCTRSn m = {
    .f.pctrs = u.f.pctrs,
  };
  *(volatile u32 *)(G.map.addr + V3D_PCTRS4) = m.w;
}

static void
write_PCTRS3(PCTRSn u) {
  PCTRSn m = {
    .f.pctrs = u.f.pctrs,
  };
  *(volatile u32 *)(G.map.addr + V3D_PCTRS3) = m.w;
}

static void
write_PCTRS2(PCTRSn u) {
  PCTRSn m = {
    .f.pctrs = u.f.pctrs,
  };
  *(volatile u32 *)(G.map.addr + V3D_PCTRS2) = m.w;
}

static void
write_PCTRS1(PCTRSn u) {
  PCTRSn m = {
    .f.pctrs = u.f.pctrs,
  };
  *(volatile u32 *)(G.map.addr + V3D_PCTRS1) = m.w;
}

static void
write_PCTRS0(PCTRSn u) {
  PCTRSn m = {
    .f.pctrs = u.f.pctrs,
  };
  *(volatile u32 *)(G.map.addr + V3D_PCTRS0) = m.w;
}

static void
write_PCTRS(PCTRS *s) {
  PCTRS m = {
    .c0.f.pctrs  = s->c0.f.pctrs,
    .c1.f.pctrs  = s->c1.f.pctrs,
    .c2.f.pctrs  = s->c2.f.pctrs,
    .c3.f.pctrs  = s->c3.f.pctrs,
    .c4.f.pctrs  = s->c4.f.pctrs,
    .c5.f.pctrs  = s->c5.f.pctrs,
    .c6.f.pctrs  = s->c6.f.pctrs,
    .c7.f.pctrs  = s->c7.f.pctrs,
    .c8.f.pctrs  = s->c8.f.pctrs,
    .c9.f.pctrs  = s->c9.f.pctrs,
    .c10.f.pctrs = s->c10.f.pctrs,
    .c11.f.pctrs = s->c11.f.pctrs,
    .c12.f.pctrs = s->c12.f.pctrs,
    .c13.f.pctrs = s->c13.f.pctrs,
    .c14.f.pctrs = s->c14.f.pctrs,
    .c15.f.pctrs = s->c15.f.pctrs,
  };
  *(volatile u32 *)(G.map.addr + V3D_PCTRS0)  = m.c0.w;
  *(volatile u32 *)(G.map.addr + V3D_PCTRS1)  = m.c1.w;
  *(volatile u32 *)(G.map.addr + V3D_PCTRS2)  = m.c2.w;
  *(volatile u32 *)(G.map.addr + V3D_PCTRS3)  = m.c3.w;
  *(volatile u32 *)(G.map.addr + V3D_PCTRS4)  = m.c4.w;
  *(volatile u32 *)(G.map.addr + V3D_PCTRS5)  = m.c5.w;
  *(volatile u32 *)(G.map.addr + V3D_PCTRS6)  = m.c6.w;
  *(volatile u32 *)(G.map.addr + V3D_PCTRS7)  = m.c7.w;
  *(volatile u32 *)(G.map.addr + V3D_PCTRS8)  = m.c8.w;
  *(volatile u32 *)(G.map.addr + V3D_PCTRS9)  = m.c9.w;
  *(volatile u32 *)(G.map.addr + V3D_PCTRS10) = m.c10.w;
  *(volatile u32 *)(G.map.addr + V3D_PCTRS11) = m.c11.w;
  *(volatile u32 *)(G.map.addr + V3D_PCTRS12) = m.c12.w;
  *(volatile u32 *)(G.map.addr + V3D_PCTRS13) = m.c13.w;
  *(volatile u32 *)(G.map.addr + V3D_PCTRS14) = m.c14.w;
  *(volatile u32 *)(G.map.addr + V3D_PCTRS15) = m.c15.w;
}

static void
write_PCTRE(PCTRE u) {
  PCTRE m = {
    .f.cten0  = u.f.cten0,
    .f.cten1  = u.f.cten1,
    .f.cten2  = u.f.cten2,
    .f.cten3  = u.f.cten3,
    .f.cten4  = u.f.cten4,
    .f.cten5  = u.f.cten5,
    .f.cten6  = u.f.cten6,
    .f.cten7  = u.f.cten7,
    .f.cten8  = u.f.cten8,
    .f.cten9  = u.f.cten9,
    .f.cten10 = u.f.cten10,
    .f.cten11 = u.f.cten11,
    .f.cten12 = u.f.cten12,
    .f.cten13 = u.f.cten13,
    .f.cten14 = u.f.cten14,
    .f.cten15 = u.f.cten15,
    .f.enable = u.f.enable,
  };
  *(volatile u32 *)(G.map.addr + V3D_PCTRE) = m.w;
}

static void
write_PCTRC(PCTRC u) {
  PCTRC m = {
    .f.ctclr0  = u.f.ctclr0,
    .f.ctclr1  = u.f.ctclr1,
    .f.ctclr2  = u.f.ctclr2,
    .f.ctclr3  = u.f.ctclr3,
    .f.ctclr4  = u.f.ctclr4,
    .f.ctclr5  = u.f.ctclr5,
    .f.ctclr6  = u.f.ctclr6,
    .f.ctclr7  = u.f.ctclr7,
    .f.ctclr8  = u.f.ctclr8,
    .f.ctclr9  = u.f.ctclr9,
    .f.ctclr10 = u.f.ctclr10,
    .f.ctclr11 = u.f.ctclr11,
    .f.ctclr12 = u.f.ctclr12,
    .f.ctclr13 = u.f.ctclr13,
    .f.ctclr14 = u.f.ctclr14,
    .f.ctclr15 = u.f.ctclr15,
  };
  *(volatile u32 *)(G.map.addr + V3D_PCTRC) = m.w;
}

static void
write_VPMBASE(VPMBASE u) {
  VPMBASE m = {
    .f.vpmursv = u.f.vpmursv,
  };
  *(volatile u32 *)(G.map.addr + V3D_VPMBASE) = m.w;
}

static void
write_VPACNTL(VPACNTL u) {
  VPACNTL m = {
    .f.vparalim = u.f.vparalim,
    .f.vpabalim = u.f.vpabalim,
    .f.vparato  = u.f.vparato,
    .f.vpabato  = u.f.vpabato,
    .f.vpalimen = u.f.vpalimen,
    .f.vpatoen  = u.f.vpatoen,
  };
  *(volatile u32 *)(G.map.addr + V3D_VPACNTL) = m.w;
}

static void
write_SRQCS(SRQCS u) {
  SRQCS m = {
    .f.qpurql   = u.f.qpurql,
    .f.qpurqerr = u.f.qpurqerr,
    .f.qpurqcm  = u.f.qpurqcm,
    .f.qpurqcc  = u.f.qpurqcc,
  };
  *(volatile u32 *)(G.map.addr + V3D_SRQCS) = m.w;
}

static void
write_SRQUL(u32 w) {
  *(volatile u32 *)(G.map.addr + V3D_SRQUL) = w;
}

static void
write_SRQUA(u32 w) {
  *(volatile u32 *)(G.map.addr + V3D_SRQUA) = w;
}

static void
write_SRQPC(u32 w) {
  *(volatile u32 *)(G.map.addr + V3D_SRQPC) = w;
}

static void
write_SQCNTL(SQCNTL u) {
  SQCNTL m = {
    .f.vsrbl = u.f.vsrbl,
    .f.csrbl = u.f.csrbl,
  };
  *(volatile u32 *)(G.map.addr + V3D_SQCNTL) = m.w;
}

static void
write_SQRSV1(SQRSV1 u) {
  *(volatile u32 *)(G.map.addr + V3D_SQRSV1) = u.w;
}

static void
write_SQRSV0(SQRSV0 u) {
  *(volatile u32 *)(G.map.addr + V3D_SQRSV0) = u.w;
}

static void
write_BXCF(BXCF u) {
  BXCF m = {
    .f.fwddisa  = u.f.fwddisa,
    .f.clipdisa = u.f.clipdisa,
  };
  *(volatile u32 *)(G.map.addr + V3D_BXCF) = m.w;
}

static void
write_BPOS(u32 w) {
  *(volatile u32 *)(G.map.addr + V3D_BPOS) = w;
}

static void
write_BPOA(u32 w) {
  *(volatile u32 *)(G.map.addr + V3D_BPOA) = w;
}

static void
write_RFC(RFC u) {
  RFC m = {
    .f.rmfct = u.f.rmfct,
  };
  *(volatile u32 *)(G.map.addr + V3D_RFC) = m.w;
}

static void
write_BFC(BFC u) {
  BFC m = {
    .f.bmfct = u.f.bmfct,
  };
  *(volatile u32 *)(G.map.addr + V3D_BFC) = m.w;
}

static void
write_CT1LC(CTnLC u) {
  CTnLC m = {
    .f.ctlslcs = u.f.ctlslcs,
    .f.ctllcm  = u.f.ctllcm,
  };
  *(volatile u32 *)(G.map.addr + V3D_CT1LC) = m.w;
}

static void
write_CT0LC(CTnLC u) {
  CTnLC m = {
    .f.ctlslcs = u.f.ctlslcs,
    .f.ctllcm  = u.f.ctllcm,
  };
  *(volatile u32 *)(G.map.addr + V3D_CT0LC) = m.w;
}

static void
write_CT1CA(u32 w) {
  *(volatile u32 *)(G.map.addr + V3D_CT1CA) = w;
}

static void
write_CT0CA(u32 w) {
  *(volatile u32 *)(G.map.addr + V3D_CT0CA) = w;
}

static void
write_CT1EA(u32 w) {
  *(volatile u32 *)(G.map.addr + V3D_CT1EA) = w;
}

static void
write_CT0EA(u32 w) {
  *(volatile u32 *)(G.map.addr + V3D_CT0EA) = w;
}

static void
write_CT1CS(CTnCS u) {
  CTnCS m = {
    .f.ctsubs = u.f.ctsubs,
    .f.ctrun  = u.f.ctrun,
    .f.ctrsta = u.f.ctrsta,
  };
  *(volatile u32 *)(G.map.addr + V3D_CT1CS) = m.w;
}

static void
write_CT0CS(CTnCS u) {
  CTnCS m = {
    .f.ctsubs = u.f.ctsubs,
    .f.ctrun  = u.f.ctrun,
    .f.ctrsta = u.f.ctrsta,
  };
  *(volatile u32 *)(G.map.addr + V3D_CT0CS) = m.w;
}

static void
write_INTDIS(INTDIS u) {
  INTDIS m = {
    .f.di_frdone   = u.f.di_frdone,
    .f.di_fldone   = u.f.di_fldone,
    .f.di_outomem  = u.f.di_outomem,
    .f.di_spilluse = u.f.di_spilluse,
  };
  *(volatile u32 *)(G.map.addr + V3D_INTDIS) = m.w;
}

static void
write_INTENA(INTENA u) {
  INTENA m = {
    .f.ei_frdone   = u.f.ei_frdone,
    .f.ei_fldone   = u.f.ei_fldone,
    .f.ei_outomem  = u.f.ei_outomem,
    .f.ei_spilluse = u.f.ei_spilluse,
  };
  *(volatile u32 *)(G.map.addr + V3D_INTENA) = m.w;
}

static void
write_INTCTL(INTCTL u) {
  INTCTL m = {
    .f.int_frdone   = u.f.int_frdone,
    .f.int_fldone   = u.f.int_fldone,
    .f.int_outomem  = u.f.int_outomem,
    .f.int_spilluse = u.f.int_spilluse,
  };
  *(volatile u32 *)(G.map.addr + V3D_INTCTL) = m.w;
}

static void
write_SLCACTL(SLCACTL u) {
  SLCACTL m = {
    .f.iccs0_to_iccs3   = u.f.iccs0_to_iccs3,
    .f.uccs0_to_uccs3   = u.f.uccs0_to_uccs3,
    .f.t0ccs0_to_t0ccs3 = u.f.t0ccs0_to_t0ccs3,
    .f.t1ccs0_to_t1ccs3 = u.f.t1ccs0_to_t1ccs3,
  };
  *(volatile u32 *)(G.map.addr + V3D_SLCACTL) = m.w;
}

static void
write_L2CACTL(L2CACTL u) {
  L2CACTL m = {
    .f.l2cena = u.f.l2cena,
    .f.l2cdis = u.f.l2cdis,
    .f.l2cclr = u.f.l2cclr,
  };
  *(volatile u32 *)(G.map.addr + V3D_L2CACTL) = m.w;
}

static void
write_SCRATCH(u32 w) {
  *(volatile u32 *)(G.map.addr + V3D_SCRATCH) = w;
}

//
// Read Helpers
//

static ERRSTAT
read_ERRSTAT(void) {
  return (ERRSTAT)(*(volatile u32 *)(G.map.addr + V3D_ERRSTAT));
}

static FDBGS
read_FDBGS(void) {
  return (FDBGS)(*(volatile u32 *)(G.map.addr + V3D_FDBGS));
}

static FDBGR
read_FDBGR(void) {
  return (FDBGR)(*(volatile u32 *)(G.map.addr + V3D_FDBGR));
}

static FDBGB
read_FDBGB(void) {
  return (FDBGB)(*(volatile u32 *)(G.map.addr + V3D_FDBGB));
}

static FDBGO
read_FDBGO(void) {
  return (FDBGO)(*(volatile u32 *)(G.map.addr + V3D_FDBGO));
}

static DBGE
read_DBGE(void) {
  return (DBGE)(*(volatile u32 *)(G.map.addr + V3D_DBGE));
}

static DBQITC
read_DBQITC(void) {
  return (DBQITC)(*(volatile u32 *)(G.map.addr + V3D_DBQITC));
}

static DBQITE
read_DBQITE(void) {
  return (DBQITE)(*(volatile u32 *)(G.map.addr + V3D_DBQITE));
}

static PCTRSn
read_PCTRS15(void) {
  return (PCTRSn)(*(volatile u32 *)(G.map.addr + V3D_PCTRS15));
}

static PCTRSn
read_PCTRS14(void) {
  return (PCTRSn)(*(volatile u32 *)(G.map.addr + V3D_PCTRS14));
}

static PCTRSn
read_PCTRS13(void) {
  return (PCTRSn)(*(volatile u32 *)(G.map.addr + V3D_PCTRS13));
}

static PCTRSn
read_PCTRS12(void) {
  return (PCTRSn)(*(volatile u32 *)(G.map.addr + V3D_PCTRS12));
}

static PCTRSn
read_PCTRS11(void) {
  return (PCTRSn)(*(volatile u32 *)(G.map.addr + V3D_PCTRS11));
}

static PCTRSn
read_PCTRS10(void) {
  return (PCTRSn)(*(volatile u32 *)(G.map.addr + V3D_PCTRS10));
}

static PCTRSn
read_PCTRS9(void) {
  return (PCTRSn)(*(volatile u32 *)(G.map.addr + V3D_PCTRS9));
}

static PCTRSn
read_PCTRS8(void) {
  return (PCTRSn)(*(volatile u32 *)(G.map.addr + V3D_PCTRS8));
}

static PCTRSn
read_PCTRS7(void) {
  return (PCTRSn)(*(volatile u32 *)(G.map.addr + V3D_PCTRS7));
}

static PCTRSn
read_PCTRS6(void) {
  return (PCTRSn)(*(volatile u32 *)(G.map.addr + V3D_PCTRS6));
}

static PCTRSn
read_PCTRS5(void) {
  return (PCTRSn)(*(volatile u32 *)(G.map.addr + V3D_PCTRS5));
}

static PCTRSn
read_PCTRS4(void) {
  return (PCTRSn)(*(volatile u32 *)(G.map.addr + V3D_PCTRS4));
}

static PCTRSn
read_PCTRS3(void) {
  return (PCTRSn)(*(volatile u32 *)(G.map.addr + V3D_PCTRS3));
}

static PCTRSn
read_PCTRS2(void) {
  return (PCTRSn)(*(volatile u32 *)(G.map.addr + V3D_PCTRS2));
}

static PCTRSn
read_PCTRS1(void) {
  return (PCTRSn)(*(volatile u32 *)(G.map.addr + V3D_PCTRS1));
}

static PCTRSn
read_PCTRS0(void) {
  return (PCTRSn)(*(volatile u32 *)(G.map.addr + V3D_PCTRS0));
}

static PCTRS
read_PCTRS(void) {
  const PCTRS s = {
    .c0.w  = *(volatile u32 *)(G.map.addr + V3D_PCTRS0),
    .c1.w  = *(volatile u32 *)(G.map.addr + V3D_PCTRS1),
    .c2.w  = *(volatile u32 *)(G.map.addr + V3D_PCTRS2),
    .c3.w  = *(volatile u32 *)(G.map.addr + V3D_PCTRS3),
    .c4.w  = *(volatile u32 *)(G.map.addr + V3D_PCTRS4),
    .c5.w  = *(volatile u32 *)(G.map.addr + V3D_PCTRS5),
    .c6.w  = *(volatile u32 *)(G.map.addr + V3D_PCTRS6),
    .c7.w  = *(volatile u32 *)(G.map.addr + V3D_PCTRS7),
    .c8.w  = *(volatile u32 *)(G.map.addr + V3D_PCTRS8),
    .c9.w  = *(volatile u32 *)(G.map.addr + V3D_PCTRS9),
    .c10.w = *(volatile u32 *)(G.map.addr + V3D_PCTRS10),
    .c11.w = *(volatile u32 *)(G.map.addr + V3D_PCTRS11),
    .c12.w = *(volatile u32 *)(G.map.addr + V3D_PCTRS12),
    .c13.w = *(volatile u32 *)(G.map.addr + V3D_PCTRS13),
    .c14.w = *(volatile u32 *)(G.map.addr + V3D_PCTRS14),
    .c15.w = *(volatile u32 *)(G.map.addr + V3D_PCTRS15),
  };
  return s;
}

static u32
read_PCTR15(void) {
  return *(volatile u32 *)(G.map.addr + V3D_PCTR15);
}

static u32
read_PCTR14(void) {
  return *(volatile u32 *)(G.map.addr + V3D_PCTR14);
}

static u32
read_PCTR13(void) {
  return *(volatile u32 *)(G.map.addr + V3D_PCTR13);
}

static u32
read_PCTR12(void) {
  return *(volatile u32 *)(G.map.addr + V3D_PCTR12);
}

static u32
read_PCTR11(void) {
  return *(volatile u32 *)(G.map.addr + V3D_PCTR11);
}

static u32
read_PCTR10(void) {
  return *(volatile u32 *)(G.map.addr + V3D_PCTR10);
}

static u32
read_PCTR9(void) {
  return *(volatile u32 *)(G.map.addr + V3D_PCTR9);
}

static u32
read_PCTR8(void) {
  return *(volatile u32 *)(G.map.addr + V3D_PCTR8);
}

static u32
read_PCTR7(void) {
  return *(volatile u32 *)(G.map.addr + V3D_PCTR7);
}

static u32
read_PCTR6(void) {
  return *(volatile u32 *)(G.map.addr + V3D_PCTR6);
}

static u32
read_PCTR5(void) {
  return *(volatile u32 *)(G.map.addr + V3D_PCTR5);
}

static u32
read_PCTR4(void) {
  return *(volatile u32 *)(G.map.addr + V3D_PCTR4);
}

static u32
read_PCTR3(void) {
  return *(volatile u32 *)(G.map.addr + V3D_PCTR3);
}

static u32
read_PCTR2(void) {
  return *(volatile u32 *)(G.map.addr + V3D_PCTR2);
}

static u32
read_PCTR1(void) {
  return *(volatile u32 *)(G.map.addr + V3D_PCTR1);
}

static u32
read_PCTR0(void) {
  return *(volatile u32 *)(G.map.addr + V3D_PCTR0);
}

static PCTR
read_PCTR(void) {
  const PCTR s = {
    .c0  = *(volatile u32 *)(G.map.addr + V3D_PCTR0),
    .c1  = *(volatile u32 *)(G.map.addr + V3D_PCTR1),
    .c2  = *(volatile u32 *)(G.map.addr + V3D_PCTR2),
    .c3  = *(volatile u32 *)(G.map.addr + V3D_PCTR3),
    .c4  = *(volatile u32 *)(G.map.addr + V3D_PCTR4),
    .c5  = *(volatile u32 *)(G.map.addr + V3D_PCTR5),
    .c6  = *(volatile u32 *)(G.map.addr + V3D_PCTR6),
    .c7  = *(volatile u32 *)(G.map.addr + V3D_PCTR7),
    .c8  = *(volatile u32 *)(G.map.addr + V3D_PCTR8),
    .c9  = *(volatile u32 *)(G.map.addr + V3D_PCTR9),
    .c10 = *(volatile u32 *)(G.map.addr + V3D_PCTR10),
    .c11 = *(volatile u32 *)(G.map.addr + V3D_PCTR11),
    .c12 = *(volatile u32 *)(G.map.addr + V3D_PCTR12),
    .c13 = *(volatile u32 *)(G.map.addr + V3D_PCTR13),
    .c14 = *(volatile u32 *)(G.map.addr + V3D_PCTR14),
    .c15 = *(volatile u32 *)(G.map.addr + V3D_PCTR15),
  };
  return s;
}

static PCTRE
read_PCTRE(void) {
  return (PCTRE)(*(volatile u32 *)(G.map.addr + V3D_PCTRE));
}

static VPMBASE
read_VPMBASE(void) {
  return (VPMBASE)(*(volatile u32 *)(G.map.addr + V3D_VPMBASE));
}

static VPACNTL
read_VPACNTL(void) {
  return (VPACNTL)(*(volatile u32 *)(G.map.addr + V3D_VPACNTL));
}

static SRQCS
read_SRQCS(void) {
  return (SRQCS)(*(volatile u32 *)(G.map.addr + V3D_SRQCS));
}

static SRQUL
read_SRQUL(void) {
  return (SRQUL)(*(volatile u32 *)(G.map.addr + V3D_SRQUL));
}

static u32
read_SRQUA(void) {
  return *(volatile u32 *)(G.map.addr + V3D_SRQUA);
}

static SQCNTL
read_SQCNTL(void) {
  return (SQCNTL)(*(volatile u32 *)(G.map.addr + V3D_SQCNTL));
}

static SQRSV1
read_SQRSV1(void) {
  return (SQRSV1)(*(volatile u32 *)(G.map.addr + V3D_SQRSV1));
}

static SQRSV0
read_SQRSV0(void) {
  return (SQRSV0)(*(volatile u32 *)(G.map.addr + V3D_SQRSV0));
}

static BXCF
read_BXCF(void) {
  return (BXCF)(*(volatile u32 *)(G.map.addr + V3D_BXCF));
}

static u32
read_BPOS(void) {
  return *(volatile u32 *)(G.map.addr + V3D_BPOS);
}

static u32
read_BPOA(void) {
  return *(volatile u32 *)(G.map.addr + V3D_BPOA);
}

static u32
read_BPCS(void) {
  return *(volatile u32 *)(G.map.addr + V3D_BPCS);
}

static u32
read_BPCA(void) {
  return *(volatile u32 *)(G.map.addr + V3D_BPCA);
}

static RFC
read_RFC(void) {
  return (RFC)(*(volatile u32 *)(G.map.addr + V3D_RFC));
}

static BFC
read_BFC(void) {
  return (BFC)(*(volatile u32 *)(G.map.addr + V3D_BFC));
}

static PCS
read_PCS(void) {
  return (PCS)(*(volatile u32 *)(G.map.addr + V3D_PCS));
}

static u32
read_CT1PC(void) {
  return *(volatile u32 *)(G.map.addr + V3D_CT1PC);
}

static u32
read_CT0PC(void) {
  return *(volatile u32 *)(G.map.addr + V3D_CT0PC);
}

static CTnLC
read_CT1LC(void) {
  return (CTnLC)(*(volatile u32 *)(G.map.addr + V3D_CT1LC));
}

static CTnLC
read_CT0LC(void) {
  return (CTnLC)(*(volatile u32 *)(G.map.addr + V3D_CT0LC));
}

static u32
read_CT01RA0(void) {
  return *(volatile u32 *)(G.map.addr + V3D_CT01RA0);
}

static u32
read_CT00RA0(void) {
  return *(volatile u32 *)(G.map.addr + V3D_CT00RA0);
}

static u32
read_CT1CA(void) {
  return *(volatile u32 *)(G.map.addr + V3D_CT1CA);
}

static u32
read_CT0CA(void) {
  return *(volatile u32 *)(G.map.addr + V3D_CT0CA);
}

static u32
read_CT1EA(void) {
  return *(volatile u32 *)(G.map.addr + V3D_CT1EA);
}

static u32
read_CT0EA(void) {
  return *(volatile u32 *)(G.map.addr + V3D_CT0EA);
}

static CTnCS
read_CT1CS(void) {
  return (CTnCS)(*(volatile u32 *)(G.map.addr + V3D_CT1CS));
}

static CTnCS
read_CT0CS(void) {
  return (CTnCS)(*(volatile u32 *)(G.map.addr + V3D_CT0CS));
}

static INTDIS
read_INTDIS(void) {
  return (INTDIS)(*(volatile u32 *)(G.map.addr + V3D_INTDIS));
}

static INTENA
read_INTENA(void) {
  return (INTENA)(*(volatile u32 *)(G.map.addr + V3D_INTENA));
}

static INTCTL
read_INTCTL(void) {
  return (INTCTL)(*(volatile u32 *)(G.map.addr + V3D_INTCTL));
}

static L2CACTL
read_L2CACTL(void) {
  return (L2CACTL)(*(volatile u32 *)(G.map.addr + V3D_L2CACTL));
}

static u32
read_SCRATCH(void) {
  return *(volatile u32 *)(G.map.addr + V3D_SCRATCH);
}

static IDENT2
read_IDENT2(void) {
  return (IDENT2)(*(volatile u32 *)(G.map.addr + V3D_IDENT2));
}

static IDENT1
read_IDENT1(void) {
  return (IDENT1)(*(volatile u32 *)(G.map.addr + V3D_IDENT1));
}

static IDENT0
read_IDENT0(void) {
  return (IDENT0)(*(volatile u32 *)(G.map.addr + V3D_IDENT0));
}

//
// Print Helpers
//

static void
print_ERRSTAT(ERRSTAT d, bool all, opt o, int fd) {
  if (o.verbose)
    DIVIDERTO(fd, "Register ERRSTAT");
  if (o.verbose || all || d.f.vpaeabb)
    LOGTO(fd, "%s: %hhu", "VPM Error Alloc While Busy", d.f.vpaeabb);
  if (o.verbose || all || d.f.vpaergs)
    LOGTO(fd, "%s: %hhu", "VPM Error Alloc Request Too Big", d.f.vpaergs);
  if (o.verbose || all || d.f.vpaebrgl)
    LOGTO(fd, "%s: %hhu", "VPM Error Alloc Binner Limit", d.f.vpaebrgl);
  if (o.verbose || all || d.f.vpaerrgl)
    LOGTO(fd, "%s: %hhu", "VPM Error Alloc Renderer Limit", d.f.vpaerrgl);
  if (o.verbose || all || d.f.vpmewr)
    LOGTO(fd, "%s: %hhu", "VPM Error Write Range", d.f.vpmewr);
  if (o.verbose || all || d.f.vpmerr)
    LOGTO(fd, "%s: %hhu", "VPM Error Read Range", d.f.vpmerr);
  if (o.verbose || all || d.f.vpmerna)
    LOGTO(fd, "%s: %hhu", "VPM Error Read Non-Alloc", d.f.vpmerna);
  if (o.verbose || all || d.f.vpmewna)
    LOGTO(fd, "%s: %hhu", "VPM Error Write Non-Alloc", d.f.vpmewna);
  if (o.verbose || all || d.f.vpmefna)
    LOGTO(fd, "%s: %hhu", "VPM Error Free Non-Alloc", d.f.vpmefna);
  if (o.verbose || all || d.f.vpmeas)
    LOGTO(fd, "%s: %hhu", "VPM Error Alloc Size", d.f.vpmeas);
  if (o.verbose || all || d.f.vdwe)
    LOGTO(fd, "%s: %hhu", "VDW Error Addr Overflow", d.f.vdwe);
  if (o.verbose || all || d.f.vcde)
    LOGTO(fd, "%s: %hhu", "VCD Error FIFO Out of Sync", d.f.vcde);
  if (o.verbose || all || d.f.vcdi)
    LOGTO(fd, "%s: %hhu", "VCD Idle", d.f.vcdi);
  if (o.verbose || all || d.f.vcmre)
    LOGTO(fd, "%s: %hhu", "VCM Error Renderer", d.f.vcmre);
  if (o.verbose || all || d.f.vcmbe)
    LOGTO(fd, "%s: %hhu", "VCM Error Binner", d.f.vcmbe);
  if (o.verbose || all || d.f.l2care)
    LOGTO(fd, "%s: %hhu", "L2C AXI FIFO Overrun Error", d.f.l2care);
}

static void
print_FDBGS(FDBGS d, bool all, opt o, int fd) {
  if (o.verbose)
    DIVIDERTO(fd, "Register FDBGS");
  if (o.verbose || all || d.f.eztest_ip_qstall)
    LOGTO(fd, "%s: %hhu", "EZTEST IP Q Stall", d.f.eztest_ip_qstall);
  if (o.verbose || all || d.f.eztest_ip_prstall)
    LOGTO(fd, "%s: %hhu", "EZTEST IP PR Stall", d.f.eztest_ip_prstall);
  if (o.verbose || all || d.f.eztest_ip_vlfstall)
    LOGTO(fd, "%s: %hhu", "EZTEST IP VLF Stall", d.f.eztest_ip_vlfstall);
  if (o.verbose || all || d.f.eztest_stall)
    LOGTO(fd, "%s: %hhu", "EZTEST Stall", d.f.eztest_stall);
  if (o.verbose || all || d.f.eztest_vlf_oknovalid)
    LOGTO(fd, "%s: %hhu", "EZTEST VLF OK No Valid", d.f.eztest_vlf_oknovalid);
  if (o.verbose || all || d.f.eztest_qready)
    LOGTO(fd, "%s: %hhu", "EZTEST Q Ready", d.f.eztest_qready);
  if (o.verbose || all || d.f.eztest_anyqf)
    LOGTO(fd, "%s: %hhu", "EZTEST Any Q F", d.f.eztest_anyqf);
  if (o.verbose || all || d.f.eztest_anyqvalid)
    LOGTO(fd, "%s: %hhu", "EZTEST Any Q Valid", d.f.eztest_anyqvalid);
  if (o.verbose || all || d.f.qxyf_fifo_op1_valid)
    LOGTO(fd, "%s: %hhu", "QXYF Fifo OP1 Valid", d.f.qxyf_fifo_op1_valid);
  if (o.verbose || all || d.f.qxyf_fifo_op1_last)
    LOGTO(fd, "%s: %hhu", "QXYF Fifo OP1 Last", d.f.qxyf_fifo_op1_last);
  if (o.verbose || all || d.f.qxyf_fifo_op1_dummy)
    LOGTO(fd, "%s: %hhu", "QXYF Fifo OP1 Dummy", d.f.qxyf_fifo_op1_dummy);
  if (o.verbose || all || d.f.qxyf_fifo_op_last)
    LOGTO(fd, "%s: %hhu", "QXYF Fifo OP Last", d.f.qxyf_fifo_op_last);
  if (o.verbose || all || d.f.qxyf_fifo_op_valid)
    LOGTO(fd, "%s: %hhu", "QXYF Fifo OP Valid", d.f.qxyf_fifo_op_valid);
  if (o.verbose || all || d.f.ezreq_fifo_op_valid)
    LOGTO(fd, "%s: %hhu", "EZREQ Fifo OP Valid", d.f.ezreq_fifo_op_valid);
  if (o.verbose || all || d.f.xynrm_ip_stall)
    LOGTO(fd, "%s: %hhu", "XYNRM IP Stall", d.f.xynrm_ip_stall);
  if (o.verbose || all || d.f.ezlim_ip_stall)
    LOGTO(fd, "%s: %hhu", "EZLIM IP Stall", d.f.ezlim_ip_stall);
  if (o.verbose || all || d.f.deptho_fifo_ip_stall)
    LOGTO(fd, "%s: %hhu", "DEPTHO Fifo IP Stall", d.f.deptho_fifo_ip_stall);
  if (o.verbose || all || d.f.interpz_ip_stall)
    LOGTO(fd, "%s: %hhu", "INTERPZ IP Stall", d.f.interpz_ip_stall);
  if (o.verbose || all || d.f.xyrelz_fifo_ip_stall)
    LOGTO(fd, "%s: %hhu", "XYRELZ Fifo IP Stall", d.f.xyrelz_fifo_ip_stall);
  if (o.verbose || all || d.f.interpw_ip_stall)
    LOGTO(fd, "%s: %hhu", "INTERPW IP Stall", d.f.interpw_ip_stall);
  if (o.verbose || all || d.f.recipw_ip_stall)
    LOGTO(fd, "%s: %hhu", "RECIPW IP Stall", d.f.recipw_ip_stall);
  if (o.verbose || all || d.f.zo_fifo_ip_stall)
    LOGTO(fd, "%s: %hhu", "ZO Fifo IP Stall", d.f.zo_fifo_ip_stall);
}

static void
print_FDBGR(FDBGR d, bool all, opt o, int fd) {
  if (o.verbose)
    DIVIDERTO(fd, "Register FDBGR");
  if (o.verbose || all || d.f.qxyf_fifo_ready)
    LOGTO(fd, "%s: %hhu", "QXYF Fifo Ready", d.f.qxyf_fifo_ready);
  if (o.verbose || all || d.f.ezreq_fifo_ready)
    LOGTO(fd, "%s: %hhu", "EZREQ Fifo Ready", d.f.ezreq_fifo_ready);
  if (o.verbose || all || d.f.ezval_fifo_ready)
    LOGTO(fd, "%s: %hhu", "EZVAL Fifo Ready", d.f.ezval_fifo_ready);
  if (o.verbose || all || d.f.deptho_fifo_ready)
    LOGTO(fd, "%s: %hhu", "DEPTHO Fifo Ready", d.f.deptho_fifo_ready);
  if (o.verbose || all || d.f.refxy_fifo_ready)
    LOGTO(fd, "%s: %hhu", "REFXY Fifo Ready", d.f.refxy_fifo_ready);
  if (o.verbose || all || d.f.zcoeff_fifo_ready)
    LOGTO(fd, "%s: %hhu", "ZCOEFF Fifo Ready", d.f.zcoeff_fifo_ready);
  if (o.verbose || all || d.f.xyrelw_fifo_ready)
    LOGTO(fd, "%s: %hhu", "XYRELW Fifo Ready", d.f.xyrelw_fifo_ready);
  if (o.verbose || all || d.f.wcoeff_fifo_ready)
    LOGTO(fd, "%s: %hhu", "WCOEFF Fifo Ready", d.f.wcoeff_fifo_ready);
  if (o.verbose || all || d.f.xyrelo_fifo_ready)
    LOGTO(fd, "%s: %hhu", "XYRELO Fifo Ready", d.f.xyrelo_fifo_ready);
  if (o.verbose || all || d.f.zo_fifo_ready)
    LOGTO(fd, "%s: %hhu", "ZO Fifo Ready", d.f.zo_fifo_ready);
  if (o.verbose || all || d.f.xyfo_fifo_ready)
    LOGTO(fd, "%s: %hhu", "XYFO Fifo Ready", d.f.xyfo_fifo_ready);
  if (o.verbose || all || d.f.rast_ready)
    LOGTO(fd, "%s: %hhu", "RAST Ready", d.f.rast_ready);
  if (o.verbose || all || d.f.rast_last)
    LOGTO(fd, "%s: %hhu", "RAST Last", d.f.rast_last);
  if (o.verbose || all || d.f.deptho_ready)
    LOGTO(fd, "%s: %hhu", "DEPTHO Ready", d.f.deptho_ready);
  if (o.verbose || all || d.f.ezlim_ready)
    LOGTO(fd, "%s: %hhu", "EZLIM Ready", d.f.ezlim_ready);
  if (o.verbose || all || d.f.xynrm_ready)
    LOGTO(fd, "%s: %hhu", "XYNRM Ready", d.f.xynrm_ready);
  if (o.verbose || all || d.f.xynrm_last)
    LOGTO(fd, "%s: %hhu", "XYNRM Last", d.f.xynrm_last);
  if (o.verbose || all || d.f.xyrelz_fifo_ready)
    LOGTO(fd, "%s: %hhu", "XYRELZ Fifo Ready", d.f.xyrelz_fifo_ready);
  if (o.verbose || all || d.f.xyrelz_fifo_last)
    LOGTO(fd, "%s: %hhu", "XYRELZ Fifo Last", d.f.xyrelz_fifo_last);
  if (o.verbose || all || d.f.interpz_ready)
    LOGTO(fd, "%s: %hhu", "INTERPZ Ready", d.f.interpz_ready);
  if (o.verbose || all || d.f.interprw_ready)
    LOGTO(fd, "%s: %hhu", "INTERPRW Ready", d.f.interprw_ready);
  if (o.verbose || all || d.f.recipw_ready)
    LOGTO(fd, "%s: %hhu", "RECIPW Ready", d.f.recipw_ready);
  if (o.verbose || all || d.f.fixz_ready)
    LOGTO(fd, "%s: %hhu", "FIXZ Ready", d.f.fixz_ready);
}

static void
print_FDBGB(FDBGB d, bool all, opt o, int fd) {
  if (o.verbose)
    DIVIDERTO(fd, "Register FDBGB");
  if (o.verbose || all || d.f.edges_stall)
    LOGTO(fd, "%s: %hhu", "Edges Stall", d.f.edges_stall);
  if (o.verbose || all || d.f.edges_ready)
    LOGTO(fd, "%s: %hhu", "Edges Ready", d.f.edges_ready);
  if (o.verbose || all || d.f.edges_isctrl)
    LOGTO(fd, "%s: %hhu", "Edges ISCTRL", d.f.edges_isctrl);
  if (o.verbose || all || d.f.edges_ctrlid)
    LOGTO(fd, "%s: %hhu", "Edges CTRLID", d.f.edges_ctrlid);
  if (o.verbose || all || d.f.zrwpe_stall)
    LOGTO(fd, "%s: %hhu", "ZRWPE Stall", d.f.zrwpe_stall);
  if (o.verbose || all || d.f.zrwpe_ready)
    LOGTO(fd, "%s: %hhu", "ZRWPE Ready", d.f.zrwpe_ready);
  if (o.verbose || all || d.f.ez_data_ready)
    LOGTO(fd, "%s: %hhu", "EZ Data Ready", d.f.ez_data_ready);
  if (o.verbose || all || d.f.ez_xy_ready)
    LOGTO(fd, "%s: %hhu", "EZ XY Ready", d.f.ez_xy_ready);
  if (o.verbose || all || d.f.rast_busy)
    LOGTO(fd, "%s: %hhu", "RAST Busy", d.f.rast_busy);
  if (o.verbose || all || d.f.qxyf_fifo_op_ready)
    LOGTO(fd, "%s: %hhu", "QXYF Fifo Op Ready", d.f.qxyf_fifo_op_ready);
  if (o.verbose || all || d.f.xyfo_fifo_op_ready)
    LOGTO(fd, "%s: %hhu", "XYFO Fifo Op Ready", d.f.xyfo_fifo_op_ready);
}

static void
print_FDBGO(FDBGO d, bool all, opt o, int fd) {
  if (o.verbose)
    DIVIDERTO(fd, "Register FDBGO");
  if (o.verbose || all || d.f.wcoeff_fifo_full)
    LOGTO(fd, "%s: %hhu", "WCOEFF Fifo Full", d.f.wcoeff_fifo_full);
  if (o.verbose || all || d.f.xyrelz_fifo_full)
    LOGTO(fd, "%s: %hhu", "XYRELZ Fifo Full", d.f.xyrelz_fifo_full);
  if (o.verbose || all || d.f.qbfr_fifo_orun)
    LOGTO(fd, "%s: %hhu", "QBFR Fifo Overrun", d.f.qbfr_fifo_orun);
  if (o.verbose || all || d.f.qbsz_fifo_orun)
    LOGTO(fd, "%s: %hhu", "QBSZ Fifo Overrun", d.f.qbsz_fifo_orun);
  if (o.verbose || all || d.f.xyfo_fifo_orun)
    LOGTO(fd, "%s: %hhu", "XYFO Fifo Overrun", d.f.xyfo_fifo_orun);
  if (o.verbose || all || d.f.fixz_orun)
    LOGTO(fd, "%s: %hhu", "FIXZ Overrun", d.f.fixz_orun);
  if (o.verbose || all || d.f.xyrelo_fifo_orun)
    LOGTO(fd, "%s: %hhu", "XYRELO Fifo Overrun", d.f.xyrelo_fifo_orun);
  if (o.verbose || all || d.f.xyrelw_fifo_orun)
    LOGTO(fd, "%s: %hhu", "XYRELW Fifo Overrun", d.f.xyrelw_fifo_orun);
  if (o.verbose || all || d.f.zcoeff_fifo_full)
    LOGTO(fd, "%s: %hhu", "ZCOEFF Fifo Overull", d.f.zcoeff_fifo_full);
  if (o.verbose || all || d.f.refxy_fifo_orun)
    LOGTO(fd, "%s: %hhu", "REFXY Fifo Overrun", d.f.refxy_fifo_orun);
  if (o.verbose || all || d.f.deptho_fifo_orun)
    LOGTO(fd, "%s: %hhu", "DEPTHO Fifo Overrun", d.f.deptho_fifo_orun);
  if (o.verbose || all || d.f.deptho_orun)
    LOGTO(fd, "%s: %hhu", "DEPTHO Overrun", d.f.deptho_orun);
  if (o.verbose || all || d.f.ezval_fifo_orun)
    LOGTO(fd, "%s: %hhu", "EZVAL Fifo Overrun", d.f.ezval_fifo_orun);
  if (o.verbose || all || d.f.ezreq_fifo_orun)
    LOGTO(fd, "%s: %hhu", "EZREQ Fifo Overrun", d.f.ezreq_fifo_orun);
}

static void
print_DBGE(DBGE d, bool all, opt o, int fd) {
  if (o.verbose)
    DIVIDERTO(fd, "Register DBGE");
  if (o.verbose || all || d.f.vr1_a)
    LOGTO(fd, "%s: %hhu", "Error A Reading VPM", d.f.vr1_a);
  if (o.verbose || all || d.f.vr1_b)
    LOGTO(fd, "%s: %hhu", "Error B Reading VPM", d.f.vr1_b);
  if (o.verbose || all || d.f.mulip0)
    LOGTO(fd, "%s: %hhu", "Error Mulip 0", d.f.mulip0);
  if (o.verbose || all || d.f.mulip1)
    LOGTO(fd, "%s: %hhu", "Error Mulip 1", d.f.mulip1);
  if (o.verbose || all || d.f.mulip2)
    LOGTO(fd, "%s: %hhu", "Error Mulip 2", d.f.mulip2);
  if (o.verbose || all || d.f.ipd2_valid)
    LOGTO(fd, "%s: %hhu", "Error IPD2 Valid", d.f.ipd2_valid);
  if (o.verbose || all || d.f.ipd2_fpdused)
    LOGTO(fd, "%s: %hhu", "Error IPD2 FPD Used", d.f.ipd2_fpdused);
}

static void
print_DBQITC(DBQITC d, bool all, opt o, int fd) {
  if (o.verbose)
    DIVIDERTO(fd, "Register DBQITC");
  if (o.verbose || all || d.f.ic_qpu0)
    LOGTO(fd, "%s: %hhu", "QPU 0 Interrupt Latched", d.f.ic_qpu0);
  if (o.verbose || all || d.f.ic_qpu1)
    LOGTO(fd, "%s: %hhu", "QPU 1 Interrupt Latched", d.f.ic_qpu1);
  if (o.verbose || all || d.f.ic_qpu2)
    LOGTO(fd, "%s: %hhu", "QPU 2 Interrupt Latched", d.f.ic_qpu2);
  if (o.verbose || all || d.f.ic_qpu3)
    LOGTO(fd, "%s: %hhu", "QPU 3 Interrupt Latched", d.f.ic_qpu3);
  if (o.verbose || all || d.f.ic_qpu4)
    LOGTO(fd, "%s: %hhu", "QPU 4 Interrupt Latched", d.f.ic_qpu4);
  if (o.verbose || all || d.f.ic_qpu5)
    LOGTO(fd, "%s: %hhu", "QPU 5 Interrupt Latched", d.f.ic_qpu5);
  if (o.verbose || all || d.f.ic_qpu6)
    LOGTO(fd, "%s: %hhu", "QPU 6 Interrupt Latched", d.f.ic_qpu6);
  if (o.verbose || all || d.f.ic_qpu7)
    LOGTO(fd, "%s: %hhu", "QPU 7 Interrupt Latched", d.f.ic_qpu7);
  if (o.verbose || all || d.f.ic_qpu8)
    LOGTO(fd, "%s: %hhu", "QPU 8 Interrupt Latched", d.f.ic_qpu8);
  if (o.verbose || all || d.f.ic_qpu9)
    LOGTO(fd, "%s: %hhu", "QPU 9 Interrupt Latched", d.f.ic_qpu9);
  if (o.verbose || all || d.f.ic_qpu10)
    LOGTO(fd, "%s: %hhu", "QPU 10 Interrupt Latched", d.f.ic_qpu10);
  if (o.verbose || all || d.f.ic_qpu11)
    LOGTO(fd, "%s: %hhu", "QPU 11 Interrupt Latched", d.f.ic_qpu11);
  if (o.verbose || all || d.f.ic_qpu12)
    LOGTO(fd, "%s: %hhu", "QPU 12 Interrupt Latched", d.f.ic_qpu12);
  if (o.verbose || all || d.f.ic_qpu13)
    LOGTO(fd, "%s: %hhu", "QPU 13 Interrupt Latched", d.f.ic_qpu13);
  if (o.verbose || all || d.f.ic_qpu14)
    LOGTO(fd, "%s: %hhu", "QPU 14 Interrupt Latched", d.f.ic_qpu14);
  if (o.verbose || all || d.f.ic_qpu15)
    LOGTO(fd, "%s: %hhu", "QPU 15 Interrupt Latched", d.f.ic_qpu15);
}

static void
print_DBQITE(DBQITE u, opt o) {
  if (o.verbose)
    DIVIDER("Register DBQITE");
  LOG("%s: %hhu", "QPU 0 Interrupt Enabled", u.f.ie_qpu0);
  LOG("%s: %hhu", "QPU 1 Interrupt Enabled", u.f.ie_qpu1);
  LOG("%s: %hhu", "QPU 2 Interrupt Enabled", u.f.ie_qpu2);
  LOG("%s: %hhu", "QPU 3 Interrupt Enabled", u.f.ie_qpu3);
  LOG("%s: %hhu", "QPU 4 Interrupt Enabled", u.f.ie_qpu4);
  LOG("%s: %hhu", "QPU 5 Interrupt Enabled", u.f.ie_qpu5);
  LOG("%s: %hhu", "QPU 6 Interrupt Enabled", u.f.ie_qpu6);
  LOG("%s: %hhu", "QPU 7 Interrupt Enabled", u.f.ie_qpu7);
  LOG("%s: %hhu", "QPU 8 Interrupt Enabled", u.f.ie_qpu8);
  LOG("%s: %hhu", "QPU 9 Interrupt Enabled", u.f.ie_qpu9);
  LOG("%s: %hhu", "QPU 10 Interrupt Enabled", u.f.ie_qpu10);
  LOG("%s: %hhu", "QPU 11 Interrupt Enabled", u.f.ie_qpu11);
  LOG("%s: %hhu", "QPU 12 Interrupt Enabled", u.f.ie_qpu12);
  LOG("%s: %hhu", "QPU 13 Interrupt Enabled", u.f.ie_qpu13);
  LOG("%s: %hhu", "QPU 14 Interrupt Enabled", u.f.ie_qpu14);
  LOG("%s: %hhu", "QPU 15 Interrupt Enabled", u.f.ie_qpu15);
}

static void
print_PCTR_PCTRS(const PCTR *d, const PCTRS *s, opt o, int fd) {
  if (o.verbose)
    DIVIDERTO(fd, "Registers PCTRn + PCTRSn");
  if (o.verbose || d->c0)
    LOGTO(fd, "%u: %s", d->c0, C(s->c0.f.pctrs));
  if (o.verbose || d->c1)
    LOGTO(fd, "%u: %s", d->c1, C(s->c1.f.pctrs));
  if (o.verbose || d->c2)
    LOGTO(fd, "%u: %s", d->c2, C(s->c2.f.pctrs));
  if (o.verbose || d->c3)
    LOGTO(fd, "%u: %s", d->c3, C(s->c3.f.pctrs));
  if (o.verbose || d->c4)
    LOGTO(fd, "%u: %s", d->c4, C(s->c4.f.pctrs));
  if (o.verbose || d->c5)
    LOGTO(fd, "%u: %s", d->c5, C(s->c5.f.pctrs));
  if (o.verbose || d->c6)
    LOGTO(fd, "%u: %s", d->c6, C(s->c6.f.pctrs));
  if (o.verbose || d->c7)
    LOGTO(fd, "%u: %s", d->c7, C(s->c7.f.pctrs));
  if (o.verbose || d->c8)
    LOGTO(fd, "%u: %s", d->c8, C(s->c8.f.pctrs));
  if (o.verbose || d->c9)
    LOGTO(fd, "%u: %s", d->c9, C(s->c9.f.pctrs));
  if (o.verbose || d->c10)
    LOGTO(fd, "%u: %s", d->c10, C(s->c10.f.pctrs));
  if (o.verbose || d->c11)
    LOGTO(fd, "%u: %s", d->c11, C(s->c11.f.pctrs));
  if (o.verbose || d->c12)
    LOGTO(fd, "%u: %s", d->c12, C(s->c12.f.pctrs));
  if (o.verbose || d->c13)
    LOGTO(fd, "%u: %s", d->c13, C(s->c13.f.pctrs));
  if (o.verbose || d->c14)
    LOGTO(fd, "%u: %s", d->c14, C(s->c14.f.pctrs));
  if (o.verbose || d->c15)
    LOGTO(fd, "%u: %s", d->c15, C(s->c15.f.pctrs));
}

static void
print_PCTRS15(PCTRSn u, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRS15");
  LOG("%s: %hhu (%s)", "Perf Map 15", u.f.pctrs, C(u.f.pctrs));
}

static void
print_PCTRS14(PCTRSn u, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRS14");
  LOG("%s: %hhu (%s)", "Perf Map 14", u.f.pctrs, C(u.f.pctrs));
}

static void
print_PCTRS13(PCTRSn u, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRS13");
  LOG("%s: %hhu (%s)", "Perf Map 13", u.f.pctrs, C(u.f.pctrs));
}

static void
print_PCTRS12(PCTRSn u, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRS12");
  LOG("%s: %hhu (%s)", "Perf Map 12", u.f.pctrs, C(u.f.pctrs));
}

static void
print_PCTRS11(PCTRSn u, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRS11");
  LOG("%s: %hhu (%s)", "Perf Map 11", u.f.pctrs, C(u.f.pctrs));
}

static void
print_PCTRS10(PCTRSn u, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRS10");
  LOG("%s: %hhu (%s)", "Perf Map 10", u.f.pctrs, C(u.f.pctrs));
}

static void
print_PCTRS9(PCTRSn u, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRS9");
  LOG("%s: %hhu (%s)", "Perf Map 9", u.f.pctrs, C(u.f.pctrs));
}

static void
print_PCTRS8(PCTRSn u, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRS8");
  LOG("%s: %hhu (%s)", "Perf Map 8", u.f.pctrs, C(u.f.pctrs));
}

static void
print_PCTRS7(PCTRSn u, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRS7");
  LOG("%s: %hhu (%s)", "Perf Map 7", u.f.pctrs, C(u.f.pctrs));
}

static void
print_PCTRS6(PCTRSn u, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRS6");
  LOG("%s: %hhu (%s)", "Perf Map 6", u.f.pctrs, C(u.f.pctrs));
}

static void
print_PCTRS5(PCTRSn u, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRS5");
  LOG("%s: %hhu (%s)", "Perf Map 5", u.f.pctrs, C(u.f.pctrs));
}

static void
print_PCTRS4(PCTRSn u, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRS4");
  LOG("%s: %hhu (%s)", "Perf Map 4", u.f.pctrs, C(u.f.pctrs));
}

static void
print_PCTRS3(PCTRSn u, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRS3");
  LOG("%s: %hhu (%s)", "Perf Map 3", u.f.pctrs, C(u.f.pctrs));
}

static void
print_PCTRS2(PCTRSn u, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRS2");
  LOG("%s: %hhu (%s)", "Perf Map 2", u.f.pctrs, C(u.f.pctrs));
}

static void
print_PCTRS1(PCTRSn u, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRS1");
  LOG("%s: %hhu (%s)", "Perf Map 1", u.f.pctrs, C(u.f.pctrs));
}

static void
print_PCTRS0(PCTRSn u, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRS0");
  LOG("%s: %hhu (%s)", "Perf Map 0", u.f.pctrs, C(u.f.pctrs));
}

static void
print_PCTRS(const PCTRS *s, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRSn");
  LOG("%s: %hhu (%s)", "Perf Map 0", s->c0.f.pctrs, C(s->c0.f.pctrs));
  LOG("%s: %hhu (%s)", "Perf Map 1", s->c1.f.pctrs, C(s->c1.f.pctrs));
  LOG("%s: %hhu (%s)", "Perf Map 2", s->c2.f.pctrs, C(s->c2.f.pctrs));
  LOG("%s: %hhu (%s)", "Perf Map 3", s->c3.f.pctrs, C(s->c3.f.pctrs));
  LOG("%s: %hhu (%s)", "Perf Map 4", s->c4.f.pctrs, C(s->c4.f.pctrs));
  LOG("%s: %hhu (%s)", "Perf Map 5", s->c5.f.pctrs, C(s->c5.f.pctrs));
  LOG("%s: %hhu (%s)", "Perf Map 6", s->c6.f.pctrs, C(s->c6.f.pctrs));
  LOG("%s: %hhu (%s)", "Perf Map 7", s->c7.f.pctrs, C(s->c7.f.pctrs));
  LOG("%s: %hhu (%s)", "Perf Map 8", s->c8.f.pctrs, C(s->c8.f.pctrs));
  LOG("%s: %hhu (%s)", "Perf Map 9", s->c9.f.pctrs, C(s->c9.f.pctrs));
  LOG("%s: %hhu (%s)", "Perf Map 10", s->c10.f.pctrs, C(s->c10.f.pctrs));
  LOG("%s: %hhu (%s)", "Perf Map 11", s->c11.f.pctrs, C(s->c11.f.pctrs));
  LOG("%s: %hhu (%s)", "Perf Map 12", s->c12.f.pctrs, C(s->c12.f.pctrs));
  LOG("%s: %hhu (%s)", "Perf Map 13", s->c13.f.pctrs, C(s->c13.f.pctrs));
  LOG("%s: %hhu (%s)", "Perf Map 14", s->c14.f.pctrs, C(s->c14.f.pctrs));
  LOG("%s: %hhu (%s)", "Perf Map 15", s->c15.f.pctrs, C(s->c15.f.pctrs));
}

static void
print_PCTR15(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTR15");
  LOG("%s: %u", "Perf Counter 15", w);
}

static void
print_PCTR14(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTR14");
  LOG("%s: %u", "Perf Counter 14", w);
}

static void
print_PCTR13(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTR13");
  LOG("%s: %u", "Perf Counter 13", w);
}

static void
print_PCTR12(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTR12");
  LOG("%s: %u", "Perf Counter 12", w);
}

static void
print_PCTR11(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTR11");
  LOG("%s: %u", "Perf Counter 11", w);
}

static void
print_PCTR10(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTR10");
  LOG("%s: %u", "Perf Counter 10", w);
}

static void
print_PCTR9(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTR9");
  LOG("%s: %u", "Perf Counter 9", w);
}

static void
print_PCTR8(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTR8");
  LOG("%s: %u", "Perf Counter 8", w);
}

static void
print_PCTR7(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTR7");
  LOG("%s: %u", "Perf Counter 7", w);
}

static void
print_PCTR6(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTR6");
  LOG("%s: %u", "Perf Counter 6", w);
}

static void
print_PCTR5(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTR5");
  LOG("%s: %u", "Perf Counter 5", w);
}

static void
print_PCTR4(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTR4");
  LOG("%s: %u", "Perf Counter 4", w);
}

static void
print_PCTR3(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTR3");
  LOG("%s: %u", "Perf Counter 3", w);
}

static void
print_PCTR2(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTR2");
  LOG("%s: %u", "Perf Counter 2", w);
}

static void
print_PCTR1(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTR1");
  LOG("%s: %u", "Perf Counter 1", w);
}

static void
print_PCTR0(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTR0");
  LOG("%s: %u", "Perf Counter 0", w);
}

static void
print_PCTR(const PCTR *s, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRn");
  LOG("%s: %u", "Perf Counter 0", s->c0);
  LOG("%s: %u", "Perf Counter 1", s->c1);
  LOG("%s: %u", "Perf Counter 2", s->c2);
  LOG("%s: %u", "Perf Counter 3", s->c3);
  LOG("%s: %u", "Perf Counter 4", s->c4);
  LOG("%s: %u", "Perf Counter 5", s->c5);
  LOG("%s: %u", "Perf Counter 6", s->c6);
  LOG("%s: %u", "Perf Counter 7", s->c7);
  LOG("%s: %u", "Perf Counter 8", s->c8);
  LOG("%s: %u", "Perf Counter 9", s->c9);
  LOG("%s: %u", "Perf Counter 10", s->c10);
  LOG("%s: %u", "Perf Counter 11", s->c11);
  LOG("%s: %u", "Perf Counter 12", s->c12);
  LOG("%s: %u", "Perf Counter 13", s->c13);
  LOG("%s: %u", "Perf Counter 14", s->c14);
  LOG("%s: %u", "Perf Counter 15", s->c15);
}

static void
print_PCTRE(PCTRE u, opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRE");
  LOG("%s: %hhu", "Perf Counter 0 Enabled", u.f.cten0);
  LOG("%s: %hhu", "Perf Counter 1 Enabled", u.f.cten1);
  LOG("%s: %hhu", "Perf Counter 2 Enabled", u.f.cten2);
  LOG("%s: %hhu", "Perf Counter 3 Enabled", u.f.cten3);
  LOG("%s: %hhu", "Perf Counter 4 Enabled", u.f.cten4);
  LOG("%s: %hhu", "Perf Counter 5 Enabled", u.f.cten5);
  LOG("%s: %hhu", "Perf Counter 6 Enabled", u.f.cten6);
  LOG("%s: %hhu", "Perf Counter 7 Enabled", u.f.cten7);
  LOG("%s: %hhu", "Perf Counter 8 Enabled", u.f.cten8);
  LOG("%s: %hhu", "Perf Counter 9 Enabled", u.f.cten9);
  LOG("%s: %hhu", "Perf Counter 10 Enabled", u.f.cten10);
  LOG("%s: %hhu", "Perf Counter 11 Enabled", u.f.cten11);
  LOG("%s: %hhu", "Perf Counter 12 Enabled", u.f.cten12);
  LOG("%s: %hhu", "Perf Counter 13 Enabled", u.f.cten13);
  LOG("%s: %hhu", "Perf Counter 14 Enabled", u.f.cten14);
  LOG("%s: %hhu", "Perf Counter 15 Enabled", u.f.cten15);
  LOG("%s: %hhu", "Master Enable (Bit 31)", u.f.enable);
}

static void
print_PCTRC(opt o) {
  if (o.verbose)
    DIVIDER("Register PCTRC");
}

static void
print_VPMBASE(VPMBASE u, opt o) {
  if (o.verbose)
    DIVIDER("Register VPMBASE");
  LOG("%s: %hhu (%u B)",
      "VPM Memory Reserved for User Programs",
      u.f.vpmursv,
      u.f.vpmursv * 256);
}

static void
print_VPACNTL(VPACNTL u, opt o) {
  if (o.verbose)
    DIVIDER("Register VPACNTL");
  LOG("%s: %hhu", "Rendering VPM Alloc Limit", u.f.vparalim);
  LOG("%s: %hhu", "Binning VPM Alloc Limit", u.f.vpabalim);
  LOG("%s: %hhu", "Rendering VPM Alloc Timeout", u.f.vparato);
  LOG("%s: %hhu", "Binning VPM Alloc Timeout", u.f.vpabato);
  LOG("%s: %hhu", "Enable VPM Alloc Limits", u.f.vpalimen);
  LOG("%s: %hhu", "Enable VPM Alloc Timeout", u.f.vpatoen);
}

static void
print_SRQCS(SRQCS d, bool all, opt o, int fd) {
  if (o.verbose)
    DIVIDERTO(fd, "Register SRQCS");
  if (o.verbose || all || d.f.qpurql)
    LOGTO(fd, "%s: %hhu", "Queue Length", d.f.qpurql);
  if (o.verbose || all || d.f.qpurqerr)
    LOGTO(fd, "%s: %hhu", "Queue Error", d.f.qpurqerr);
  if (o.verbose || all || d.f.qpurqcm)
    LOGTO(fd, "%s: %hhu", "User Program Requests", d.f.qpurqcm);
  if (o.verbose || all || d.f.qpurqcc)
    LOGTO(fd, "%s: %hhu", "User Programs Completed", d.f.qpurqcc);
}

static void
print_SRQUL(SRQUL u, opt o) {
  if (o.verbose)
    DIVIDER("Register SRQUL");
  LOG("%s: %hhu", "Uniforms Length", u.f.qpurqul);
}

static void
print_SRQUA(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register SRQUA");
  LOG("%s: %#x", "Uniforms Address", w);
}

static void
print_SRQPC(opt o) {
  if (o.verbose)
    DIVIDER("Register SRQPC");
}

static void
print_SQCNTL(SQCNTL u, opt o) {
  if (o.verbose)
    DIVIDER("Register SQCNTL");
  LOG("%s: %hhu", "Vertex Shader Scheduling Bypass Limit", u.f.vsrbl);
  LOG("%s: %hhu", "Coordinate Shader Scheduling Bypass Limit", u.f.csrbl);
}

static void
print_SQRSV1(SQRSV1 u, opt o) {
  if (o.verbose)
    DIVIDER("Register SQRSV1");
  LOG("%s: %#x (%s)", "QPU 8 Reservation", u.f.qpursv8, R(u.f.qpursv8));
  LOG("%s: %#x (%s)", "QPU 9 Reservation", u.f.qpursv9, R(u.f.qpursv9));
  LOG("%s: %#x (%s)", "QPU 10 Reservation", u.f.qpursv10, R(u.f.qpursv10));
  LOG("%s: %#x (%s)", "QPU 11 Reservation", u.f.qpursv11, R(u.f.qpursv11));
  LOG("%s: %#x (%s)", "QPU 12 Reservation", u.f.qpursv12, R(u.f.qpursv12));
  LOG("%s: %#x (%s)", "QPU 13 Reservation", u.f.qpursv13, R(u.f.qpursv13));
  LOG("%s: %#x (%s)", "QPU 14 Reservation", u.f.qpursv14, R(u.f.qpursv14));
  LOG("%s: %#x (%s)", "QPU 15 Reservation", u.f.qpursv15, R(u.f.qpursv15));
}

static void
print_SQRSV0(SQRSV0 u, opt o) {
  if (o.verbose)
    DIVIDER("Register SQRSV0");
  LOG("%s: %#x (%s)", "QPU 0 Reservation", u.f.qpursv0, R(u.f.qpursv0));
  LOG("%s: %#x (%s)", "QPU 1 Reservation", u.f.qpursv1, R(u.f.qpursv1));
  LOG("%s: %#x (%s)", "QPU 2 Reservation", u.f.qpursv2, R(u.f.qpursv2));
  LOG("%s: %#x (%s)", "QPU 3 Reservation", u.f.qpursv3, R(u.f.qpursv3));
  LOG("%s: %#x (%s)", "QPU 4 Reservation", u.f.qpursv4, R(u.f.qpursv4));
  LOG("%s: %#x (%s)", "QPU 5 Reservation", u.f.qpursv5, R(u.f.qpursv5));
  LOG("%s: %#x (%s)", "QPU 6 Reservation", u.f.qpursv6, R(u.f.qpursv6));
  LOG("%s: %#x (%s)", "QPU 7 Reservation", u.f.qpursv7, R(u.f.qpursv7));
}

static void
print_BXCF(BXCF u, opt o) {
  if (o.verbose)
    DIVIDER("Register BXCF");
  LOG("%s: %hhu", "Disable Forwarding in State Cache", u.f.fwddisa);
  LOG("%s: %hhu", "Disable Clipping", u.f.clipdisa);
}

static void
print_BPOS(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register BPOS");
  LOG("%s: %u", "Size of Overspill Memory Block for Binning", w);
}

static void
print_BPOA(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register BPOA");
  LOG("%s: %#x", "Address of Overspill Memory Block for Binning", w);
}

static void
print_BPCS(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register BPCS");
  LOG("%s: %u", "Size of Pool Remaining", w);
}

static void
print_BPCA(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register BPCA");
  LOG("%s: %#x", "Current Pool Address", w);
}

static void
print_RFC(RFC u, opt o) {
  if (o.verbose)
    DIVIDER("Register RFC");
  LOG("%s: %hhu", "Flush Count", u.f.rmfct);
}

static void
print_BFC(BFC u, opt o) {
  if (o.verbose)
    DIVIDER("Register BFC");
  LOG("%s: %hhu", "Flush Count", u.f.bmfct);
}

static void
print_PCS(PCS u, opt o) {
  if (o.verbose)
    DIVIDER("Register PCS");
  LOG("%s: %hhu", "Binning Mode Active", u.f.bmactive);
  LOG("%s: %hhu", "Binning Mode Busy", u.f.bmbusy);
  LOG("%s: %hhu", "Rendering Mode Active", u.f.rmactive);
  LOG("%s: %hhu", "Rendering Mode Busy", u.f.rmbusy);
  LOG("%s: %hhu", "Binning Mode Out of Memory", u.f.bmoom);
}

static void
print_CT1PC(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register CT1PC");
  LOG("%s: %#x", "Primitive List Counter", w);
}

static void
print_CT0PC(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register CT0PC");
  LOG("%s: %#x", "Primitive List Counter", w);
}

static void
print_CT1LC(CTnLC u, opt o) {
  if (o.verbose)
    DIVIDER("Register CT1LC");
  LOG("%s: %hhu", "Sub-list Counter", u.f.ctlslcs);
  LOG("%s: %hhu", "Major List Counter", u.f.ctllcm);
}

static void
print_CT0LC(CTnLC u, opt o) {
  if (o.verbose)
    DIVIDER("Register CT0LC");
  LOG("%s: %hhu", "Sub-list Counter", u.f.ctlslcs);
  LOG("%s: %hhu", "Major List Counter", u.f.ctllcm);
}

static void
print_CT01RA0(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register CT01RA0");
  LOG("%s: %#x", "Control List Return Address 0", w);
}

static void
print_CT00RA0(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register CT00RA0");
  LOG("%s: %#x", "Control List Return Address 0", w);
}

static void
print_CT1CA(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register CT1CA");
  LOG("%s: %#x", "Control List Current Address", w);
}

static void
print_CT0CA(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register CT0CA");
  LOG("%s: %#x", "Control List Current Address", w);
}

static void
print_CT1EA(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register CT1EA");
  LOG("%s: %#x", "Control List End Address", w);
}

static void
print_CT0EA(u32 w, opt o) {
  if (o.verbose)
    DIVIDER("Register CT0EA");
  LOG("%s: %#x", "Control List End Address", w);
}

static void
print_CT1CS(CTnCS u, opt o) {
  if (o.verbose)
    DIVIDER("Register CT1CS");
  LOG("%s: %hhu", "Control Thread Mode", u.f.ctmode);
  LOG("%s: %hhu", "Control Thread Error", u.f.cterr);
  LOG("%s: %hhu", "Control Thread Sub-mode", u.f.ctsubs);
  LOG("%s: %hhu", "Control Thread Run", u.f.ctrun);
  LOG("%s: %hhu", "Return Stack Depth", u.f.ctrtsd);
  LOG("%s: %hhu", "Counting Semaphore", u.f.ctsema);
}

static void
print_CT0CS(CTnCS u, opt o) {
  if (o.verbose)
    DIVIDER("Register CT0CS");
  LOG("%s: %hhu", "Control Thread Mode", u.f.ctmode);
  LOG("%s: %hhu", "Control Thread Error", u.f.cterr);
  LOG("%s: %hhu", "Control Thread Sub-mode", u.f.ctsubs);
  LOG("%s: %hhu", "Control Thread Run", u.f.ctrun);
  LOG("%s: %hhu", "Return Stack Depth", u.f.ctrtsd);
  LOG("%s: %hhu", "Counting Semaphore", u.f.ctsema);
}

static void
print_INTDIS(INTDIS u, opt o) {
  if (o.verbose)
    DIVIDER("Register INTDIS");
  LOG("%s: %hhu", "Render Mode Frame Done", u.f.di_frdone);
  LOG("%s: %hhu", "Binning Mode Flush Done", u.f.di_fldone);
  LOG("%s: %hhu", "Binner Out of Memory", u.f.di_outomem);
  LOG("%s: %hhu", "Binner Used Overspill Memory", u.f.di_spilluse);
}

static void
print_INTENA(INTENA u, opt o) {
  if (o.verbose)
    DIVIDER("Register INTENA");
  LOG("%s: %hhu", "Render Mode Frame Done", u.f.ei_frdone);
  LOG("%s: %hhu", "Binning Mode Flush Done", u.f.ei_fldone);
  LOG("%s: %hhu", "Binner Out of Memory", u.f.ei_outomem);
  LOG("%s: %hhu", "Binner Used Overspill Memory", u.f.ei_spilluse);
}

static void
print_INTCTL(INTCTL u, opt o) {
  if (o.verbose)
    DIVIDER("Register INTCTL");
  LOG("%s: %hhu", "Render Mode Frame Done", u.f.int_frdone);
  LOG("%s: %hhu", "Binning Mode Flush Done", u.f.int_fldone);
  LOG("%s: %hhu", "Binner Out of Memory", u.f.int_outomem);
  LOG("%s: %hhu", "Binner Used Overspill Memory", u.f.int_spilluse);
}

static void
print_SLCACTL(opt o) {
  if (o.verbose)
    DIVIDER("Register SLCACTL");
}

static void
print_L2CACTL(L2CACTL u, opt o) {
  if (o.verbose)
    DIVIDER("Register L2CACTL");
  LOG("%s: %hhu", "L2 Cache Enabled", u.f.l2cena);
}

static void
print_SCRATCH(u32 w, bool all, opt o, int fd) {
  if (o.verbose)
    DIVIDERTO(fd, "Register SCRATCH");
  if (o.verbose || all || w)
    LOGTO(fd, "%s: %#x", "Scratch Register", w);
}

static void
print_IDENT2(IDENT2 u, opt o) {
  if (o.verbose)
    DIVIDER("Register IDENT2");
  LOG("%s: %hhu", "VRI Memory Size", u.f.vrisz);
  LOG("%s: %hhu", "Tile Buffer Size", u.f.tlbsz);
  LOG("%s: %hhu", "Double-buffer Mode Support", u.f.tlbdb);
}

static void
print_IDENT1(IDENT1 u, opt o) {
  if (o.verbose)
    DIVIDER("Register IDENT1");
  LOG("%s: %hhu", "V3D Revision", u.f.rev);
  LOG("%s: %hhu", "Slices", u.f.nslc);
  LOG("%s: %hhu", "QPUs per Slice", u.f.qups);
  LOG("%s: %hhu", "TMUs per Slice", u.f.tups);
  LOG("%s: %hhu", "Semaphores", u.f.nsem);
  LOG("%s: %hhu", "HDR Support", u.f.hdrt);
  LOG("%s: %hhu (%u KiB)", "VPM Memory Size", u.f.vpmsz, u.f.vpmsz);
}

static void
print_IDENT0(IDENT0 u, opt o) {
  if (o.verbose)
    DIVIDER("Register IDENT0");
  LOG("%s: %c%c%c", "ID String", u.f.idstr0, u.f.idstr1, u.f.idstr2);
  LOG("%s: %hhu", "Technology Version", u.f.tver);
}

static void
print_debug(const debug *s, opt o) {
  print_ERRSTAT(s->errstat, false, o, STDERR_FILENO);
  print_FDBGS(s->fdbgs, false, o, STDERR_FILENO);
  print_FDBGR(s->fdbgr, false, o, STDERR_FILENO);
  print_FDBGB(s->fdbgb, false, o, STDERR_FILENO);
  print_FDBGO(s->fdbgo, false, o, STDERR_FILENO);
  print_DBGE(s->dbge, false, o, STDERR_FILENO);
  print_DBQITC(s->dbqitc, false, o, STDERR_FILENO);
  print_SRQCS(s->srqcs, false, o, STDERR_FILENO);
  print_SCRATCH(s->scratch, false, o, STDERR_FILENO);
}

//
// Diff Helpers
//

static ERRSTAT
diff_ERRSTAT(ERRSTAT a, ERRSTAT b) {
  ERRSTAT diff;

  D(f.vpaeabb);
  D(f.vpaergs);
  D(f.vpaebrgl);
  D(f.vpaerrgl);
  D(f.vpmewr);
  D(f.vpmerr);
  D(f.vpmerna);
  D(f.vpmewna);
  D(f.vpmefna);
  D(f.vpmeas);
  D(f.vdwe);
  D(f.vcde);
  D(f.vcdi);
  D(f.vcmre);
  D(f.vcmbe);
  D(f.l2care);

  return diff;
}

static FDBGS
diff_FDBGS(FDBGS a, FDBGS b) {
  FDBGS diff;

  D(f.eztest_ip_qstall);
  D(f.eztest_ip_prstall);
  D(f.eztest_ip_vlfstall);
  D(f.eztest_stall);
  D(f.eztest_vlf_oknovalid);
  D(f.eztest_qready);
  D(f.eztest_anyqf);
  D(f.eztest_anyqvalid);
  D(f.qxyf_fifo_op1_valid);
  D(f.qxyf_fifo_op1_last);
  D(f.qxyf_fifo_op1_dummy);
  D(f.qxyf_fifo_op_last);
  D(f.qxyf_fifo_op_valid);
  D(f.ezreq_fifo_op_valid);
  D(f.xynrm_ip_stall);
  D(f.ezlim_ip_stall);
  D(f.deptho_fifo_ip_stall);
  D(f.interpz_ip_stall);
  D(f.xyrelz_fifo_ip_stall);
  D(f.interpw_ip_stall);
  D(f.recipw_ip_stall);
  D(f.zo_fifo_ip_stall);

  return diff;
}

static FDBGR
diff_FDBGR(FDBGR a, FDBGR b) {
  FDBGR diff;

  D(f.qxyf_fifo_ready);
  D(f.ezreq_fifo_ready);
  D(f.ezval_fifo_ready);
  D(f.deptho_fifo_ready);
  D(f.refxy_fifo_ready);
  D(f.zcoeff_fifo_ready);
  D(f.xyrelw_fifo_ready);
  D(f.wcoeff_fifo_ready);
  D(f.xyrelo_fifo_ready);
  D(f.zo_fifo_ready);
  D(f.xyfo_fifo_ready);
  D(f.rast_ready);
  D(f.rast_last);
  D(f.deptho_ready);
  D(f.ezlim_ready);
  D(f.xynrm_ready);
  D(f.xynrm_last);
  D(f.xyrelz_fifo_ready);
  D(f.xyrelz_fifo_last);
  D(f.interpz_ready);
  D(f.interprw_ready);
  D(f.recipw_ready);
  D(f.fixz_ready);

  return diff;
}

static FDBGB
diff_FDBGB(FDBGB a, FDBGB b) {
  FDBGB diff;

  D(f.edges_stall);
  D(f.edges_ready);
  D(f.edges_isctrl);
  D(f.edges_ctrlid);
  D(f.zrwpe_stall);
  D(f.zrwpe_ready);
  D(f.ez_data_ready);
  D(f.ez_xy_ready);
  D(f.rast_busy);
  D(f.qxyf_fifo_op_ready);
  D(f.xyfo_fifo_op_ready);

  return diff;
}

static FDBGO
diff_FDBGO(FDBGO a, FDBGO b) {
  FDBGO diff;

  D(f.wcoeff_fifo_full);
  D(f.xyrelz_fifo_full);
  D(f.qbfr_fifo_orun);
  D(f.qbsz_fifo_orun);
  D(f.xyfo_fifo_orun);
  D(f.fixz_orun);
  D(f.xyrelo_fifo_orun);
  D(f.xyrelw_fifo_orun);
  D(f.zcoeff_fifo_full);
  D(f.refxy_fifo_orun);
  D(f.deptho_fifo_orun);
  D(f.deptho_orun);
  D(f.ezval_fifo_orun);
  D(f.ezreq_fifo_orun);

  return diff;
}

static DBGE
diff_DBGE(DBGE a, DBGE b) {
  DBGE diff;

  D(f.vr1_a);
  D(f.vr1_b);
  D(f.mulip0);
  D(f.mulip1);
  D(f.mulip2);
  D(f.ipd2_valid);
  D(f.ipd2_fpdused);

  return diff;
}

static DBQITC
diff_DBQITC(DBQITC a, DBQITC b) {
  DBQITC diff;

  D(f.ic_qpu0);
  D(f.ic_qpu1);
  D(f.ic_qpu2);
  D(f.ic_qpu3);
  D(f.ic_qpu4);
  D(f.ic_qpu5);
  D(f.ic_qpu6);
  D(f.ic_qpu7);
  D(f.ic_qpu8);
  D(f.ic_qpu9);
  D(f.ic_qpu10);
  D(f.ic_qpu11);
  D(f.ic_qpu12);
  D(f.ic_qpu13);
  D(f.ic_qpu14);
  D(f.ic_qpu15);

  return diff;
}

static PCTR
diff_PCTR(PCTR a, PCTR b) {
  PCTR diff;

  D(c0);
  D(c1);
  D(c2);
  D(c3);
  D(c4);
  D(c5);
  D(c6);
  D(c7);
  D(c8);
  D(c9);
  D(c10);
  D(c11);
  D(c12);
  D(c13);
  D(c14);
  D(c15);

  return diff;
}

static SRQCS
diff_SRQCS(SRQCS a, SRQCS b) {
  SRQCS diff;

  D(f.qpurql);
  D(f.qpurqerr);
  D(f.qpurqcm);
  D(f.qpurqcc);

  return diff;
}

static u32
diff_SCRATCH(u32 a, u32 b) {
  return b - a;
}

static void
diff_debug(debug *diff, const debug *a, const debug *b) {
  diff->errstat = diff_ERRSTAT(a->errstat, b->errstat);
  diff->fdbgs   = diff_FDBGS(a->fdbgs, b->fdbgs);
  diff->fdbgr   = diff_FDBGR(a->fdbgr, b->fdbgr);
  diff->fdbgb   = diff_FDBGB(a->fdbgb, b->fdbgb);
  diff->fdbgo   = diff_FDBGO(a->fdbgo, b->fdbgo);
  diff->dbge    = diff_DBGE(a->dbge, b->dbge);
  diff->dbqitc  = diff_DBQITC(a->dbqitc, b->dbqitc);
  diff->srqcs   = diff_SRQCS(a->srqcs, b->srqcs);
  diff->scratch = diff_SCRATCH(a->scratch, b->scratch);
}

//
// Init Helpers
//

static void
enable_counters(void) {
  CHECK_ENABLED();

  PCTRE u = {
    .f.cten0  = 1,
    .f.cten1  = 1,
    .f.cten2  = 1,
    .f.cten3  = 1,
    .f.cten4  = 1,
    .f.cten5  = 1,
    .f.cten6  = 1,
    .f.cten7  = 1,
    .f.cten8  = 1,
    .f.cten9  = 1,
    .f.cten10 = 1,
    .f.cten11 = 1,
    .f.cten12 = 1,
    .f.cten13 = 1,
    .f.cten14 = 1,
    .f.cten15 = 1,
    .f.enable = 1,
  };

  write_PCTRE(u);
}

static void
select_counters(void) {
  CHECK_ENABLED();

  PCTRS s = {
    .c0.f.pctrs  = 13,
    .c1.f.pctrs  = 14,
    .c2.f.pctrs  = 15,
    .c3.f.pctrs  = 16,
    .c4.f.pctrs  = 17,
    .c5.f.pctrs  = 19,
    .c6.f.pctrs  = 20,
    .c7.f.pctrs  = 21,
    .c8.f.pctrs  = 22,
    .c9.f.pctrs  = 23,
    .c10.f.pctrs = 24,
    .c11.f.pctrs = 25,
    .c12.f.pctrs = 26,
    .c13.f.pctrs = 27,
    .c14.f.pctrs = 28,
    .c15.f.pctrs = 29,
  };

  write_PCTRS(&s);
}

//
// Stats
//

void
reg_debug_print(opt o) {
  debug diff;
  diff_debug(&diff, &G.debug.before, &G.debug.after);
  print_debug(&diff, o);
}

void
reg_debug_after(void) {
  G.debug.after.errstat = read_ERRSTAT();
  G.debug.after.fdbgs   = read_FDBGS();
  G.debug.after.fdbgr   = read_FDBGR();
  G.debug.after.fdbgb   = read_FDBGB();
  G.debug.after.fdbgo   = read_FDBGO();
  G.debug.after.dbge    = read_DBGE();
  G.debug.after.dbqitc  = read_DBQITC();
  G.debug.after.srqcs   = read_SRQCS();
  G.debug.after.scratch = read_SCRATCH();
}

void
reg_debug_before(void) {
  G.debug.before.errstat = read_ERRSTAT();
  G.debug.before.fdbgs   = read_FDBGS();
  G.debug.before.fdbgr   = read_FDBGR();
  G.debug.before.fdbgb   = read_FDBGB();
  G.debug.before.fdbgo   = read_FDBGO();
  G.debug.before.dbge    = read_DBGE();
  G.debug.before.dbqitc  = read_DBQITC();
  G.debug.before.srqcs   = read_SRQCS();
  G.debug.before.scratch = read_SCRATCH();
}

void
reg_perf_print(opt o) {
  const PCTRS pctrs = read_PCTRS();
  const PCTR diff   = diff_PCTR(G.perf.before, G.perf.after);
  print_PCTR_PCTRS(&diff, &pctrs, o, STDERR_FILENO);
}

void
reg_perf_after(void) {
  G.perf.after = read_PCTR();
}

void
reg_perf_before(void) {
  G.perf.before = read_PCTR();
}

//
// Execute
//

void
reg_enable_irqs(void) {
  CHECK_ENABLED();

  DBQITE u = {
    .f.ie_qpu0  = 1,
    .f.ie_qpu1  = 1,
    .f.ie_qpu2  = 1,
    .f.ie_qpu3  = 1,
    .f.ie_qpu4  = 1,
    .f.ie_qpu5  = 1,
    .f.ie_qpu6  = 1,
    .f.ie_qpu7  = 1,
    .f.ie_qpu8  = 1,
    .f.ie_qpu9  = 1,
    .f.ie_qpu10 = 1,
    .f.ie_qpu11 = 1,
    .f.ie_qpu12 = 1,
    .f.ie_qpu13 = 1,
    .f.ie_qpu14 = 1,
    .f.ie_qpu15 = 1,
  };

  write_DBQITE(u);
}

void
reg_enable_counters(void) {
  CHECK_ENABLED();

  PCTRE u = {
    .f.cten0  = 1,
    .f.cten1  = 1,
    .f.cten2  = 1,
    .f.cten3  = 1,
    .f.cten4  = 1,
    .f.cten5  = 1,
    .f.cten6  = 1,
    .f.cten7  = 1,
    .f.cten8  = 1,
    .f.cten9  = 1,
    .f.cten10 = 1,
    .f.cten11 = 1,
    .f.cten12 = 1,
    .f.cten13 = 1,
    .f.cten14 = 1,
    .f.cten15 = 1,
    .f.enable = 1,
  };

  write_PCTRE(u);
}

void
reg_select_counters(void) {
  CHECK_ENABLED();

  PCTRS s = {
    .c0.f.pctrs  = 13,
    .c1.f.pctrs  = 14,
    .c2.f.pctrs  = 15,
    .c3.f.pctrs  = 16,
    .c4.f.pctrs  = 17,
    .c5.f.pctrs  = 19,
    .c6.f.pctrs  = 20,
    .c7.f.pctrs  = 21,
    .c8.f.pctrs  = 22,
    .c9.f.pctrs  = 23,
    .c10.f.pctrs = 24,
    .c11.f.pctrs = 25,
    .c12.f.pctrs = 26,
    .c13.f.pctrs = 27,
    .c14.f.pctrs = 28,
    .c15.f.pctrs = 29,
  };

  write_PCTRS(&s);
}

void
reg_reserve_vpm(void) {
  CHECK_ENABLED();

  VPMBASE u = {
    u.f.vpmursv = ~0,
  };

  write_VPMBASE(u);
}

void
reg_reserve_qpus(void) {
  CHECK_ENABLED();

  enum {
    NOUSER = 0b0001,
    NOFRAG = 0b0010,
    NOVERT = 0b0100,
    NOCOOR = 0b1000,
  };

  SQRSV0 u0 = {
    .f.qpursv0 = (NOFRAG | NOVERT | NOCOOR),
    .f.qpursv1 = (NOFRAG | NOVERT | NOCOOR),
    .f.qpursv2 = (NOFRAG | NOVERT | NOCOOR),
    .f.qpursv3 = (NOFRAG | NOVERT | NOCOOR),
    .f.qpursv4 = (NOFRAG | NOVERT | NOCOOR),
    .f.qpursv5 = (NOFRAG | NOVERT | NOCOOR),
    .f.qpursv6 = (NOFRAG | NOVERT | NOCOOR),
    .f.qpursv7 = (NOFRAG | NOVERT | NOCOOR),
  };

  SQRSV1 u1 = {
    .f.qpursv8  = (NOFRAG | NOVERT | NOCOOR),
    .f.qpursv9  = (NOFRAG | NOVERT | NOCOOR),
    .f.qpursv10 = (NOFRAG | NOVERT | NOCOOR),
    .f.qpursv11 = (NOFRAG | NOVERT | NOCOOR),
    .f.qpursv12 = (NOFRAG | NOVERT | NOCOOR),
    .f.qpursv13 = (NOFRAG | NOVERT | NOCOOR),
    .f.qpursv14 = (NOFRAG | NOVERT | NOCOOR),
    .f.qpursv15 = (NOFRAG | NOVERT | NOCOOR),
  };

  write_SQRSV0(u0);
  write_SQRSV1(u1);
}

//
// Init
//

void
reg_init_pctr() {
  select_counters();
  enable_counters();
}

//
// Write
//

void
reg_write_errstat(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_fdbgs(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_fdbgr(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_fdbgb(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_fdbgo(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_dbge(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_dbqitc(u32 w) {
  CHECK_ENABLED();

  write_DBQITC((DBQITC)w);
}

void
reg_write_dbqite(u32 w) {
  CHECK_ENABLED();

  write_DBQITE((DBQITE)w);
}

void
reg_write_pctrs15(u32 w) {
  CHECK_ENABLED();

  write_PCTRS15((PCTRSn)w);
}

void
reg_write_pctrs14(u32 w) {
  CHECK_ENABLED();

  write_PCTRS14((PCTRSn)w);
}

void
reg_write_pctrs13(u32 w) {
  CHECK_ENABLED();

  write_PCTRS13((PCTRSn)w);
}

void
reg_write_pctrs12(u32 w) {
  CHECK_ENABLED();

  write_PCTRS12((PCTRSn)w);
}

void
reg_write_pctrs11(u32 w) {
  CHECK_ENABLED();

  write_PCTRS11((PCTRSn)w);
}

void
reg_write_pctrs10(u32 w) {
  CHECK_ENABLED();

  write_PCTRS10((PCTRSn)w);
}

void
reg_write_pctrs9(u32 w) {
  CHECK_ENABLED();

  write_PCTRS9((PCTRSn)w);
}

void
reg_write_pctrs8(u32 w) {
  CHECK_ENABLED();

  write_PCTRS8((PCTRSn)w);
}

void
reg_write_pctrs7(u32 w) {
  CHECK_ENABLED();

  write_PCTRS7((PCTRSn)w);
}

void
reg_write_pctrs6(u32 w) {
  CHECK_ENABLED();

  write_PCTRS6((PCTRSn)w);
}

void
reg_write_pctrs5(u32 w) {
  CHECK_ENABLED();

  write_PCTRS5((PCTRSn)w);
}

void
reg_write_pctrs4(u32 w) {
  CHECK_ENABLED();

  write_PCTRS4((PCTRSn)w);
}

void
reg_write_pctrs3(u32 w) {
  CHECK_ENABLED();

  write_PCTRS3((PCTRSn)w);
}

void
reg_write_pctrs2(u32 w) {
  CHECK_ENABLED();

  write_PCTRS2((PCTRSn)w);
}

void
reg_write_pctrs1(u32 w) {
  CHECK_ENABLED();

  write_PCTRS1((PCTRSn)w);
}

void
reg_write_pctrs0(u32 w) {
  CHECK_ENABLED();

  write_PCTRS0((PCTRSn)w);
}

void
reg_write_pctrs(u32 w) {
  CHECK_ENABLED();

  PCTRS s = {
    .c0.w  = w,
    .c1.w  = w,
    .c2.w  = w,
    .c3.w  = w,
    .c4.w  = w,
    .c5.w  = w,
    .c6.w  = w,
    .c7.w  = w,
    .c8.w  = w,
    .c9.w  = w,
    .c10.w = w,
    .c11.w = w,
    .c12.w = w,
    .c13.w = w,
    .c14.w = w,
    .c15.w = w,
  };

  write_PCTRS(&s);
}

void
reg_write_pctr15(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_pctr14(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_pctr13(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_pctr12(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_pctr11(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_pctr10(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_pctr9(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_pctr8(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_pctr7(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_pctr6(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_pctr5(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_pctr4(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_pctr3(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_pctr2(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_pctr1(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_pctr0(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_pctr(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_pctre(u32 w) {
  CHECK_ENABLED();

  write_PCTRE((PCTRE)w);
}

void
reg_write_pctrc(u32 w) {
  CHECK_ENABLED();

  write_PCTRC((PCTRC)w);
}

void
reg_write_vpmbase(u32 w) {
  CHECK_ENABLED();

  write_VPMBASE((VPMBASE)w);
}

void
reg_write_vpacntl(u32 w) {
  CHECK_ENABLED();

  write_VPACNTL((VPACNTL)w);
}

void
reg_write_srqcs(u32 w) {
  CHECK_ENABLED();

  write_SRQCS((SRQCS)w);
}

void
reg_write_srqul(u32 w) {
  CHECK_ENABLED();

  write_SRQUL(w);
}

void
reg_write_srqua(u32 w) {
  CHECK_ENABLED();

  write_SRQUA(w);
}

void
reg_write_srqpc(u32 w) {
  CHECK_ENABLED();

  write_SRQPC(w);
}

void
reg_write_sqcntl(u32 w) {
  CHECK_ENABLED();

  write_SQCNTL((SQCNTL)w);
}

void
reg_write_sqrsv1(u32 w) {
  CHECK_ENABLED();

  write_SQRSV1((SQRSV1)w);
}

void
reg_write_sqrsv0(u32 w) {
  CHECK_ENABLED();

  write_SQRSV0((SQRSV0)w);
}

void
reg_write_bxcf(u32 w) {
  CHECK_ENABLED();

  write_BXCF((BXCF)w);
}

void
reg_write_bpos(u32 w) {
  CHECK_ENABLED();

  write_BPOS(w);
}

void
reg_write_bpoa(u32 w) {
  CHECK_ENABLED();

  write_BPOA(w);
}

void
reg_write_bpcs(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_bpca(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_rfc(u32 w) {
  CHECK_ENABLED();

  write_RFC((RFC)w);
}

void
reg_write_bfc(u32 w) {
  CHECK_ENABLED();

  write_BFC((BFC)w);
}

void
reg_write_pcs(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_ct1pc(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_ct0pc(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_ct1lc(u32 w) {
  CHECK_ENABLED();

  write_CT1LC((CTnLC)w);
}

void
reg_write_ct0lc(u32 w) {
  CHECK_ENABLED();

  write_CT0LC((CTnLC)w);
}

void
reg_write_ct01ra0(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_ct00ra0(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_ct1ca(u32 w) {
  CHECK_ENABLED();

  write_CT1CA(w);
}

void
reg_write_ct0ca(u32 w) {
  CHECK_ENABLED();

  write_CT0CA(w);
}

void
reg_write_ct1ea(u32 w) {
  CHECK_ENABLED();

  write_CT1EA(w);
}

void
reg_write_ct0ea(u32 w) {
  CHECK_ENABLED();

  write_CT0EA(w);
}

void
reg_write_ct1cs(u32 w) {
  CHECK_ENABLED();

  write_CT1CS((CTnCS)w);
}

void
reg_write_ct0cs(u32 w) {
  CHECK_ENABLED();

  write_CT0CS((CTnCS)w);
}

void
reg_write_intdis(u32 w) {
  CHECK_ENABLED();

  write_INTDIS((INTDIS)w);
}

void
reg_write_intena(u32 w) {
  CHECK_ENABLED();

  write_INTENA((INTENA)w);
}

void
reg_write_intctl(u32 w) {
  CHECK_ENABLED();

  write_INTCTL((INTCTL)w);
}

void
reg_write_slcactl(u32 w) {
  CHECK_ENABLED();

  write_SLCACTL((SLCACTL)w);
}

void
reg_write_l2cactl(u32 w) {
  CHECK_ENABLED();

  write_L2CACTL((L2CACTL)w);
}

void
reg_write_scratch(u32 w) {
  CHECK_ENABLED();

  write_SCRATCH(w);
}

void
reg_write_ident2(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_ident1(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

void
reg_write_ident0(u32 w) {
  CHECK_ENABLED();

  NOTICE("Write not supported");
}

//
// Read
//

void
reg_read_errstat(opt o) {
  CHECK_ENABLED();

  const ERRSTAT u = read_ERRSTAT();
  print_ERRSTAT(u, true, o, STDOUT_FILENO);
}

void
reg_read_fdbgs(opt o) {
  CHECK_ENABLED();

  const FDBGS u = read_FDBGS();
  print_FDBGS(u, true, o, STDOUT_FILENO);
}

void
reg_read_fdbgr(opt o) {
  CHECK_ENABLED();

  const FDBGR u = read_FDBGR();
  print_FDBGR(u, true, o, STDOUT_FILENO);
}

void
reg_read_fdbgb(opt o) {
  CHECK_ENABLED();

  const FDBGB u = read_FDBGB();
  print_FDBGB(u, true, o, STDOUT_FILENO);
}

void
reg_read_fdbgo(opt o) {
  CHECK_ENABLED();

  const FDBGO u = read_FDBGO();
  print_FDBGO(u, true, o, STDOUT_FILENO);
}

void
reg_read_dbge(opt o) {
  CHECK_ENABLED();

  const DBGE u = read_DBGE();
  print_DBGE(u, true, o, STDOUT_FILENO);
}

void
reg_read_dbqitc(opt o) {
  CHECK_ENABLED();

  const DBQITC u = read_DBQITC();
  print_DBQITC(u, true, o, STDOUT_FILENO);
}

void
reg_read_dbqite(opt o) {
  CHECK_ENABLED();

  const DBQITE u = read_DBQITE();
  print_DBQITE(u, o);
}

void
reg_read_pctrs15(opt o) {
  CHECK_ENABLED();

  const PCTRSn u = read_PCTRS15();
  print_PCTRS15(u, o);
}

void
reg_read_pctrs14(opt o) {
  CHECK_ENABLED();

  const PCTRSn u = read_PCTRS14();
  print_PCTRS14(u, o);
}

void
reg_read_pctrs13(opt o) {
  CHECK_ENABLED();

  const PCTRSn u = read_PCTRS13();
  print_PCTRS13(u, o);
}

void
reg_read_pctrs12(opt o) {
  CHECK_ENABLED();

  const PCTRSn u = read_PCTRS12();
  print_PCTRS12(u, o);
}

void
reg_read_pctrs11(opt o) {
  CHECK_ENABLED();

  const PCTRSn u = read_PCTRS11();
  print_PCTRS11(u, o);
}

void
reg_read_pctrs10(opt o) {
  CHECK_ENABLED();

  const PCTRSn u = read_PCTRS10();
  print_PCTRS10(u, o);
}

void
reg_read_pctrs9(opt o) {
  CHECK_ENABLED();

  const PCTRSn u = read_PCTRS9();
  print_PCTRS9(u, o);
}

void
reg_read_pctrs8(opt o) {
  CHECK_ENABLED();

  const PCTRSn u = read_PCTRS8();
  print_PCTRS8(u, o);
}

void
reg_read_pctrs7(opt o) {
  CHECK_ENABLED();

  const PCTRSn u = read_PCTRS7();
  print_PCTRS7(u, o);
}

void
reg_read_pctrs6(opt o) {
  CHECK_ENABLED();

  const PCTRSn u = read_PCTRS6();
  print_PCTRS6(u, o);
}

void
reg_read_pctrs5(opt o) {
  CHECK_ENABLED();

  const PCTRSn u = read_PCTRS5();
  print_PCTRS5(u, o);
}

void
reg_read_pctrs4(opt o) {
  CHECK_ENABLED();

  const PCTRSn u = read_PCTRS4();
  print_PCTRS4(u, o);
}

void
reg_read_pctrs3(opt o) {
  CHECK_ENABLED();

  const PCTRSn u = read_PCTRS3();
  print_PCTRS3(u, o);
}

void
reg_read_pctrs2(opt o) {
  CHECK_ENABLED();

  const PCTRSn u = read_PCTRS2();
  print_PCTRS2(u, o);
}

void
reg_read_pctrs1(opt o) {
  CHECK_ENABLED();

  const PCTRSn u = read_PCTRS1();
  print_PCTRS1(u, o);
}

void
reg_read_pctrs0(opt o) {
  CHECK_ENABLED();

  const PCTRSn u = read_PCTRS0();
  print_PCTRS0(u, o);
}

void
reg_read_pctrs(opt o) {
  CHECK_ENABLED();

  const PCTRS s = read_PCTRS();
  print_PCTRS(&s, o);
}

void
reg_read_pctr15(opt o) {
  CHECK_ENABLED();

  const u32 u = read_PCTR15();
  print_PCTR15(u, o);
}

void
reg_read_pctr14(opt o) {
  CHECK_ENABLED();

  const u32 u = read_PCTR14();
  print_PCTR14(u, o);
}

void
reg_read_pctr13(opt o) {
  CHECK_ENABLED();

  const u32 u = read_PCTR13();
  print_PCTR13(u, o);
}

void
reg_read_pctr12(opt o) {
  CHECK_ENABLED();

  const u32 u = read_PCTR12();
  print_PCTR12(u, o);
}

void
reg_read_pctr11(opt o) {
  CHECK_ENABLED();

  const u32 u = read_PCTR11();
  print_PCTR11(u, o);
}

void
reg_read_pctr10(opt o) {
  CHECK_ENABLED();

  const u32 u = read_PCTR10();
  print_PCTR10(u, o);
}

void
reg_read_pctr9(opt o) {
  CHECK_ENABLED();

  const u32 u = read_PCTR9();
  print_PCTR9(u, o);
}

void
reg_read_pctr8(opt o) {
  CHECK_ENABLED();

  const u32 u = read_PCTR8();
  print_PCTR8(u, o);
}

void
reg_read_pctr7(opt o) {
  CHECK_ENABLED();

  const u32 u = read_PCTR7();
  print_PCTR7(u, o);
}

void
reg_read_pctr6(opt o) {
  CHECK_ENABLED();

  const u32 u = read_PCTR6();
  print_PCTR6(u, o);
}

void
reg_read_pctr5(opt o) {
  CHECK_ENABLED();

  const u32 u = read_PCTR5();
  print_PCTR5(u, o);
}

void
reg_read_pctr4(opt o) {
  CHECK_ENABLED();

  const u32 u = read_PCTR4();
  print_PCTR4(u, o);
}

void
reg_read_pctr3(opt o) {
  CHECK_ENABLED();

  const u32 u = read_PCTR3();
  print_PCTR3(u, o);
}

void
reg_read_pctr2(opt o) {
  CHECK_ENABLED();

  const u32 u = read_PCTR2();
  print_PCTR2(u, o);
}

void
reg_read_pctr1(opt o) {
  CHECK_ENABLED();

  const u32 u = read_PCTR1();
  print_PCTR1(u, o);
}

void
reg_read_pctr0(opt o) {
  CHECK_ENABLED();

  const u32 u = read_PCTR0();
  print_PCTR0(u, o);
}

void
reg_read_pctr(opt o) {
  CHECK_ENABLED();

  const PCTR s = read_PCTR();
  print_PCTR(&s, o);
}

void
reg_read_pctre(opt o) {
  CHECK_ENABLED();

  const PCTRE u = read_PCTRE();
  print_PCTRE(u, o);
}

void
reg_read_pctrc(opt o) {
  CHECK_ENABLED();

  print_PCTRC(o);
  NOTICE("Read not supported");
}

void
reg_read_vpmbase(opt o) {
  CHECK_ENABLED();

  const VPMBASE u = read_VPMBASE();
  print_VPMBASE(u, o);
}

void
reg_read_vpacntl(opt o) {
  CHECK_ENABLED();

  const VPACNTL u = read_VPACNTL();
  print_VPACNTL(u, o);
}

void
reg_read_srqcs(opt o) {
  CHECK_ENABLED();

  const SRQCS u = read_SRQCS();
  print_SRQCS(u, true, o, STDOUT_FILENO);
}

void
reg_read_srqul(opt o) {
  CHECK_ENABLED();

  const SRQUL u = read_SRQUL();
  print_SRQUL(u, o);
}

void
reg_read_srqua(opt o) {
  CHECK_ENABLED();

  const u32 w = read_SRQUA();
  print_SRQUA(w, o);
}

void
reg_read_srqpc(opt o) {
  CHECK_ENABLED();

  print_SRQPC(o);
  NOTICE("Read not supported");
}

void
reg_read_sqcntl(opt o) {
  CHECK_ENABLED();

  const SQCNTL u = read_SQCNTL();
  print_SQCNTL(u, o);
}

void
reg_read_sqrsv1(opt o) {
  CHECK_ENABLED();

  const SQRSV1 u = read_SQRSV1();
  print_SQRSV1(u, o);
}

void
reg_read_sqrsv0(opt o) {
  CHECK_ENABLED();

  const SQRSV0 u = read_SQRSV0();
  print_SQRSV0(u, o);
}

void
reg_read_bxcf(opt o) {
  CHECK_ENABLED();

  const BXCF u = read_BXCF();
  print_BXCF(u, o);
}

void
reg_read_bpos(opt o) {
  CHECK_ENABLED();

  const u32 w = read_BPOS();
  print_BPOS(w, o);
}

void
reg_read_bpoa(opt o) {
  CHECK_ENABLED();

  const u32 w = read_BPOA();
  print_BPOA(w, o);
}

void
reg_read_bpcs(opt o) {
  CHECK_ENABLED();

  const u32 w = read_BPCS();
  print_BPCS(w, o);
}

void
reg_read_bpca(opt o) {
  CHECK_ENABLED();

  const u32 w = read_BPCA();
  print_BPCA(w, o);
}

void
reg_read_rfc(opt o) {
  CHECK_ENABLED();

  const RFC u = read_RFC();
  print_RFC(u, o);
}

void
reg_read_bfc(opt o) {
  CHECK_ENABLED();

  const BFC u = read_BFC();
  print_BFC(u, o);
}

void
reg_read_pcs(opt o) {
  CHECK_ENABLED();

  const PCS u = read_PCS();
  print_PCS(u, o);
}

void
reg_read_ct1pc(opt o) {
  CHECK_ENABLED();

  const u32 w = read_CT1PC();
  print_CT1PC(w, o);
}

void
reg_read_ct0pc(opt o) {
  CHECK_ENABLED();

  const u32 w = read_CT0PC();
  print_CT0PC(w, o);
}

void
reg_read_ct1lc(opt o) {
  CHECK_ENABLED();

  const CTnLC u = read_CT1LC();
  print_CT1LC(u, o);
}

void
reg_read_ct0lc(opt o) {
  CHECK_ENABLED();

  const CTnLC u = read_CT0LC();
  print_CT0LC(u, o);
}

void
reg_read_ct01ra0(opt o) {
  CHECK_ENABLED();

  const u32 w = read_CT01RA0();
  print_CT01RA0(w, o);
}

void
reg_read_ct00ra0(opt o) {
  CHECK_ENABLED();

  const u32 w = read_CT00RA0();
  print_CT00RA0(w, o);
}

void
reg_read_ct1ca(opt o) {
  CHECK_ENABLED();

  const u32 w = read_CT1CA();
  print_CT1CA(w, o);
}

void
reg_read_ct0ca(opt o) {
  CHECK_ENABLED();

  const u32 w = read_CT0CA();
  print_CT0CA(w, o);
}

void
reg_read_ct1ea(opt o) {
  CHECK_ENABLED();

  const u32 w = read_CT1EA();
  print_CT1EA(w, o);
}

void
reg_read_ct0ea(opt o) {
  CHECK_ENABLED();

  const u32 w = read_CT0EA();
  print_CT0EA(w, o);
}

void
reg_read_ct1cs(opt o) {
  CHECK_ENABLED();

  const CTnCS u = read_CT1CS();
  print_CT1CS(u, o);
}

void
reg_read_ct0cs(opt o) {
  CHECK_ENABLED();

  const CTnCS u = read_CT0CS();
  print_CT0CS(u, o);
}

void
reg_read_intdis(opt o) {
  CHECK_ENABLED();

  const INTDIS u = read_INTDIS();
  print_INTDIS(u, o);
}

void
reg_read_intena(opt o) {
  CHECK_ENABLED();

  const INTENA u = read_INTENA();
  print_INTENA(u, o);
}

void
reg_read_intctl(opt o) {
  CHECK_ENABLED();

  const INTCTL u = read_INTCTL();
  print_INTCTL(u, o);
}

void
reg_read_slcactl(opt o) {
  CHECK_ENABLED();

  print_SLCACTL(o);
  NOTICE("Read not supported");
}

void
reg_read_l2cactl(opt o) {
  CHECK_ENABLED();

  const L2CACTL u = read_L2CACTL();
  print_L2CACTL(u, o);
}

void
reg_read_scratch(opt o) {
  CHECK_ENABLED();

  const u32 w = read_SCRATCH();
  print_SCRATCH(w, true, o, STDOUT_FILENO);
}

void
reg_read_ident2(opt o) {
  CHECK_ENABLED();

  const IDENT2 u = read_IDENT2();
  print_IDENT2(u, o);
}

void
reg_read_ident1(opt o) {
  CHECK_ENABLED();

  const IDENT1 u = read_IDENT1();
  print_IDENT1(u, o);
}

void
reg_read_ident0(opt o) {
  CHECK_ENABLED();

  const IDENT0 u = read_IDENT0();
  print_IDENT0(u, o);
}

//
// Misc
//

void
reg_print_perf(void) {
  LOG("PERFORMANCE COUNTERS");
  for (u32 i = 0; i < nperfctr; ++i) {
    LOG("  %2u: %s", perfctr[i].id, perfctr[i].desc);
  }
}

void
reg_print_reg(void) {
  const char *s =
    "REGISTERS                                                            \n"
    "  V3D Identity                                                       \n"
    "    ident0        R    V3D Identification 0                          \n"
    "    ident1        R    V3D Identification 1                          \n"
    "    ident2        R    V3D Identification 2                          \n"
    "  V3D Miscellaneous                                                  \n"
    "    scratch       RW   Scratch Register                              \n"
    "  Cache Control                                                      \n"
    "    l2cactl       RW   L2 Cache Control                              \n"
    "    slcactl        W   Slices Cache Control                          \n"
    "  Pipeline Interrupt Control                                         \n"
    "    intctl        RW   Pipeline Interrupt Control                    \n"
    "    intena        RW   Pipeline Interrupt Enables                    \n"
    "    intdis        RW   Pipeline Interrupt Disables                   \n"
    "  Control List Executor                                              \n"
    "    ct0cs         RW   Thread 0 Control and Status                   \n"
    "    ct1cs         RW   Thread 1 Control and Status                   \n"
    "    ct0ea         RW   Thread 0 End Address                          \n"
    "    ct1ea         RW   Thread 1 End Address                          \n"
    "    ct0ca         RW   Thread 0 Current Address                      \n"
    "    ct1ca         RW   Thread 1 Current Address                      \n"
    "    ct00ra0       R    Thread 0 Return Address 0                     \n"
    "    ct01ra0       R    Thread 1 Return Address 0                     \n"
    "    ct0lc         RW   Thread 0 List Counter                         \n"
    "    ct1lc         RW   Thread 1 List Counter                         \n"
    "    ct0pc         R    Thread 0 Primitive List Counter               \n"
    "    ct1pc         R    Thread 1 Primitive List Counter               \n"
    "  V3D Pipeline                                                       \n"
    "    pcs           R    Pipeline Control and Status                   \n"
    "    bfc           RW   Binning Mode Flush Count                      \n"
    "    rfc           RW   Rendering Mode Frame Count                    \n"
    "    bpca          R    Current Address of Binning Memory Pool        \n"
    "    bpcs          R    Remaining Size of Binning Memory Pool         \n"
    "    bpoa          RW   Address of Overspill Binning Memory Block     \n"
    "    bpos          RW   Size of Overspill Binning Memory Block        \n"
    "    bxcf          RW   Binner Debug                                  \n"
    "  QPU Scheduler                                                      \n"
    "    sqrsv0        RW   Reserve QPUs 0–7                              \n"
    "    sqrsv1        RW   Reserve QPUs 8–15                             \n"
    "    sqcntl        RW   QPU Scheduler Control                         \n"
    "    srqpc          W   QPU User Program Request Program Address      \n"
    "    srqua         RW   QPU User Program Request Uniforms Address     \n"
    "    srqul         RW   QPU User Program Request Uniforms Length      \n"
    "    srqcs         RW   QPU User Program Request Control and Status   \n"
    "  VPM                                                                \n"
    "    vpacntl       RW   VPM Allocator Control                         \n"
    "    vpmbase       RW   VPM Base User Memory Reservation              \n"
    "  Performance Counters                                               \n"
    "    pctrc          W   Perf Counter Clear                            \n"
    "    pctre         RW   Perf Counter Enables                          \n"
    "    pctr          R    Perf Counters (All)                           \n"
    "    pctr<0..15>   R    Perf Counter                                  \n"
    "    pctrs         RW   Perf Counter ID Mappings (All)                \n"
    "    pctrs<0..15>  RW   Perf Counter ID Mapping                       \n"
    "  QPU Interrupt Control                                              \n"
    "    dbqite        RW   QPU Interrupt Enables                         \n"
    "    dbqitc        RW   QPU Interrupt Control                         \n"
    "  Errors and Diagnostics                                             \n"
    "    dbge          R    PSE Error Signals                             \n"
    "    fdbgo         R    FEP Overrun Error Signals                     \n"
    "    fdbgb         R    FEP Ready, Stall, and Busy Signals            \n"
    "    fdbgr         R    FEP Internal Ready Signals                    \n"
    "    fdbgs         R    FEP Internal Stall Input Signals              \n"
    "    errstat       R    Miscellaneous Error Signals                  ";
  LOG("%s", s);
}

bool
reg_gpu_is_enabled(void) {
  const union32 u = {.b = {'V', '3', 'D', 2}};
  const u32 id    = *(volatile u32 *)(G.map.addr + V3D_IDENT0);
  return (id == u.w);
}

//
// Init
//

result
reg_cleanup(void) {
  bool error = false;

  if (G.map.addr) {
    result r = mem_unmap(G.map.addr, G.map.sz);
    if (r != SUCCESS) {
      error = true;
    }
    G.map.addr = 0;
    G.map.sz   = 0;
  }

  return error ? FAILURE : SUCCESS;
}

result
reg_init(void) {
  uaddr virt;

  uaddr phys = bcm_host_get_peripheral_address();
  u32 size   = bcm_host_get_peripheral_size();

  result r = mem_map(&virt, phys, size);
  if (r != SUCCESS) {
    return FAILURE;
  }

  G.map.addr = virt;
  G.map.sz   = size;

  return SUCCESS;
}
