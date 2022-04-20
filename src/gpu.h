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

result gpu_init(void);
result gpu_cleanup(void);

bool gpu_has_task(void);
void gpu_set_timeout(u32);

result gpu_glob_unif(const char *);
result gpu_glob_rbuf(const char *);
result gpu_glob_wbuf(u32);

result gpu_next_task(void);
result gpu_task_inst(const char *);
result gpu_task_unif(const char *);
result gpu_task_rbuf(const char *);
result gpu_task_wbuf(u32);
result gpu_replicate(u32 mult);

result gpu_exec_via_mbox(opt);
result gpu_exec_via_regs(opt);
