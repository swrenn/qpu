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

result mbox_init(void);
result mbox_cleanup(void);

result mbox_enable(opt);
result mbox_disable(opt);
result mbox_board(opt);
result mbox_clocks(opt);
result mbox_memory(opt);
result mbox_power(opt);
result mbox_temp(opt);
result mbox_version(opt);
result mbox_voltage(opt);

result mbox_alloc(u32 *, u32, u32);
result mbox_free(u32);
result mbox_lock(uaddr *, u32);
result mbox_unlock(u32);
result mbox_exec_qpu(u32, uaddr, bool, u32);
