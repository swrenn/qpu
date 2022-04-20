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

#include <stdbool.h>
#include <stdint.h>

#define timespecsub(result, a, b)                    \
  do {                                               \
    (result)->tv_sec  = (b)->tv_sec - (a)->tv_sec;   \
    (result)->tv_nsec = (b)->tv_nsec - (a)->tv_nsec; \
    if ((result)->tv_nsec < 0) {                     \
      --(result)->tv_sec;                            \
      (result)->tv_nsec += 1000 * 1000 * 1000;       \
    }                                                \
  } while (0)

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef u32 uaddr;

typedef enum result {
  SUCCESS,
  FAILURE,
  ABORT,
} result;

typedef union union64 {
  u64 l;
  u32 w[2];
  u8 b[8];
} __attribute__((packed)) union64;

typedef union union32 {
  u32 w;
  u8 b[4];
} __attribute__((packed)) union32;

typedef struct opt {
  bool dry;
  bool dump0;
  bool dump1;
  bool executing;
  bool isatty;
  bool mctr0;
  bool mctr1;
  bool mdebug;
  bool mtime;
  bool verbose;
  u32 timeout_s;
} opt;
