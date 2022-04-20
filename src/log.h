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

#include <libgen.h>
#include <stdio.h>

static const int width = 32;

#define LOGTO(fd, str, ...)               \
  do {                                    \
    dprintf(fd, str "\n", ##__VA_ARGS__); \
  } while (0)

#define NOTICETO(fd, str, ...)            \
  do {                                    \
    dprintf(fd, str "\n", ##__VA_ARGS__); \
  } while (0)

#define ERRORTO(fd, str, ...)           \
  do {                                  \
    dprintf(fd,                         \
            "ERROR (%s:%u): " str "\n", \
            basename(__FILE__),         \
            __LINE__,                   \
            ##__VA_ARGS__);             \
  } while (0)

#define DIVIDERTO(fd, str)                                    \
  do {                                                        \
    char buf[width + 1], *pbuf = buf, *pstr = str, len, a, b; \
    len = strlen(str);                                        \
    len = len > width - 4 ? width - 4 : len;                  \
    a   = (width - len) / 2 - 1;                              \
    b   = (width - len + 1) / 2 - 1;                          \
    for (int i = 0; i < a; ++i)                               \
      *pbuf++ = '-';                                          \
    *pbuf++ = ' ';                                            \
    while (pbuf - buf < width - 1 - b)                        \
      *pbuf++ = *pstr++;                                      \
    *pbuf++ = ' ';                                            \
    for (int i = 0; i < b; ++i)                               \
      *pbuf++ = '-';                                          \
    *pbuf = '\0';                                             \
    dprintf(fd, "%s\n", buf);                                 \
  } while (0)

#define LOG(str, ...) LOGTO(STDOUT_FILENO, str, ##__VA_ARGS__)
#define NOTICE(str, ...) NOTICETO(STDERR_FILENO, str, ##__VA_ARGS__)
#define ERROR(str, ...) ERRORTO(STDERR_FILENO, str, ##__VA_ARGS__)
#define DIVIDER(str, ...) DIVIDERTO(STDOUT_FILENO, str, ##__VA_ARGS__)
