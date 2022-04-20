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

#include "mem.h"

#include "log.h"
#include "types.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

result
mem_unmap(uaddr virt, u32 size) {
  int ret = munmap((void *)virt, size);
  if (ret == -1) {
    ERROR("%s", strerror(errno));
    return FAILURE;
  }

  return SUCCESS;
}

result
mem_map(uaddr *virt, uaddr phys, u32 size) {
  int fd = open("/dev/mem", O_RDWR | O_SYNC);
  if (fd == -1) {
    if (errno == EACCES) {
      NOTICE("Need root");
      return FAILURE;
    } else {
      ERROR("%s", strerror(errno));
      return FAILURE;
    }
  }

  void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, phys);
  if (p == MAP_FAILED) {
    ERROR("%s", strerror(errno));
    return FAILURE;
  }

  int ret = close(fd);
  if (ret == -1) {
    ERROR("%s", strerror(errno));
    return FAILURE;
  }

  *virt = (uaddr)p;

  return SUCCESS;
}
