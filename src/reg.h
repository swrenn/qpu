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

result reg_init(void);
result reg_cleanup(void);

bool reg_gpu_is_enabled(void);

void reg_print_reg(void);
void reg_print_perf(void);

void reg_read_ident0(opt);
void reg_read_ident1(opt);
void reg_read_ident2(opt);
void reg_read_scratch(opt);
void reg_read_l2cactl(opt);
void reg_read_slcactl(opt);
void reg_read_intctl(opt);
void reg_read_intena(opt);
void reg_read_intdis(opt);
void reg_read_ct0cs(opt);
void reg_read_ct1cs(opt);
void reg_read_ct0ea(opt);
void reg_read_ct1ea(opt);
void reg_read_ct0ca(opt);
void reg_read_ct1ca(opt);
void reg_read_ct00ra0(opt);
void reg_read_ct01ra0(opt);
void reg_read_ct0lc(opt);
void reg_read_ct1lc(opt);
void reg_read_ct0pc(opt);
void reg_read_ct1pc(opt);
void reg_read_pcs(opt);
void reg_read_bfc(opt);
void reg_read_rfc(opt);
void reg_read_bpca(opt);
void reg_read_bpcs(opt);
void reg_read_bpoa(opt);
void reg_read_bpos(opt);
void reg_read_bxcf(opt);
void reg_read_sqrsv0(opt);
void reg_read_sqrsv1(opt);
void reg_read_sqcntl(opt);
void reg_read_srqpc(opt);
void reg_read_srqua(opt);
void reg_read_srqul(opt);
void reg_read_srqcs(opt);
void reg_read_vpacntl(opt);
void reg_read_vpmbase(opt);
void reg_read_pctrc(opt);
void reg_read_pctre(opt);
void reg_read_pctr(opt);
void reg_read_pctr0(opt);
void reg_read_pctr1(opt);
void reg_read_pctr2(opt);
void reg_read_pctr3(opt);
void reg_read_pctr4(opt);
void reg_read_pctr5(opt);
void reg_read_pctr6(opt);
void reg_read_pctr7(opt);
void reg_read_pctr8(opt);
void reg_read_pctr9(opt);
void reg_read_pctr10(opt);
void reg_read_pctr11(opt);
void reg_read_pctr12(opt);
void reg_read_pctr13(opt);
void reg_read_pctr14(opt);
void reg_read_pctr15(opt);
void reg_read_pctrs(opt);
void reg_read_pctrs0(opt);
void reg_read_pctrs1(opt);
void reg_read_pctrs2(opt);
void reg_read_pctrs3(opt);
void reg_read_pctrs4(opt);
void reg_read_pctrs5(opt);
void reg_read_pctrs6(opt);
void reg_read_pctrs7(opt);
void reg_read_pctrs8(opt);
void reg_read_pctrs9(opt);
void reg_read_pctrs10(opt);
void reg_read_pctrs11(opt);
void reg_read_pctrs12(opt);
void reg_read_pctrs13(opt);
void reg_read_pctrs14(opt);
void reg_read_pctrs15(opt);
void reg_read_dbqite(opt);
void reg_read_dbqitc(opt);
void reg_read_dbge(opt);
void reg_read_fdbgo(opt);
void reg_read_fdbgb(opt);
void reg_read_fdbgr(opt);
void reg_read_fdbgs(opt);
void reg_read_errstat(opt);

void reg_write_ident0(u32);
void reg_write_ident1(u32);
void reg_write_ident2(u32);
void reg_write_scratch(u32);
void reg_write_l2cactl(u32);
void reg_write_slcactl(u32);
void reg_write_intctl(u32);
void reg_write_intena(u32);
void reg_write_intdis(u32);
void reg_write_ct0cs(u32);
void reg_write_ct1cs(u32);
void reg_write_ct0ea(u32);
void reg_write_ct1ea(u32);
void reg_write_ct0ca(u32);
void reg_write_ct1ca(u32);
void reg_write_ct00ra0(u32);
void reg_write_ct01ra0(u32);
void reg_write_ct0lc(u32);
void reg_write_ct1lc(u32);
void reg_write_ct0pc(u32);
void reg_write_ct1pc(u32);
void reg_write_pcs(u32);
void reg_write_bfc(u32);
void reg_write_rfc(u32);
void reg_write_bpca(u32);
void reg_write_bpcs(u32);
void reg_write_bpoa(u32);
void reg_write_bpos(u32);
void reg_write_bxcf(u32);
void reg_write_sqrsv0(u32);
void reg_write_sqrsv1(u32);
void reg_write_sqcntl(u32);
void reg_write_srqpc(u32);
void reg_write_srqua(u32);
void reg_write_srqul(u32);
void reg_write_srqcs(u32);
void reg_write_vpacntl(u32);
void reg_write_vpmbase(u32);
void reg_write_pctrc(u32);
void reg_write_pctre(u32);
void reg_write_pctr(u32);
void reg_write_pctr0(u32);
void reg_write_pctr1(u32);
void reg_write_pctr2(u32);
void reg_write_pctr3(u32);
void reg_write_pctr4(u32);
void reg_write_pctr5(u32);
void reg_write_pctr6(u32);
void reg_write_pctr7(u32);
void reg_write_pctr8(u32);
void reg_write_pctr9(u32);
void reg_write_pctr10(u32);
void reg_write_pctr11(u32);
void reg_write_pctr12(u32);
void reg_write_pctr13(u32);
void reg_write_pctr14(u32);
void reg_write_pctr15(u32);
void reg_write_pctrs(u32);
void reg_write_pctrs0(u32);
void reg_write_pctrs1(u32);
void reg_write_pctrs2(u32);
void reg_write_pctrs3(u32);
void reg_write_pctrs4(u32);
void reg_write_pctrs5(u32);
void reg_write_pctrs6(u32);
void reg_write_pctrs7(u32);
void reg_write_pctrs8(u32);
void reg_write_pctrs9(u32);
void reg_write_pctrs10(u32);
void reg_write_pctrs11(u32);
void reg_write_pctrs12(u32);
void reg_write_pctrs13(u32);
void reg_write_pctrs14(u32);
void reg_write_pctrs15(u32);
void reg_write_dbqite(u32);
void reg_write_dbqitc(u32);
void reg_write_dbge(u32);
void reg_write_fdbgo(u32);
void reg_write_fdbgb(u32);
void reg_write_fdbgr(u32);
void reg_write_fdbgs(u32);
void reg_write_errstat(u32);

void reg_init_pctr(void);

void reg_perf_before(void);
void reg_perf_after(void);
void reg_perf_print(opt);

void reg_debug_before(void);
void reg_debug_after(void);
void reg_debug_print(opt);
