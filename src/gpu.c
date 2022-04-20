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

#include "gpu.h"

#include "log.h"
#include "mbox.h"
#include "mem.h"
#include "reg.h"
#include "types.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define ROUNDUP(from, to) ((((to) + (from)-1) / (to)) * (to))

enum {
  MAX_TASKS = 12,
};

typedef struct control {
  struct {
    uaddr unif;
    uaddr inst;
  } task[MAX_TASKS];
} control;

typedef struct gpu_file {
  bool active;
  u32 fd;
  u32 size;
  u32 offset;
} gpu_file;

typedef struct gpu_buf {
  bool active;
  u32 size;
  u32 offset;
} gpu_buf;

typedef struct gpu_mem {
  struct {
    bool alloc;
    bool lock;
    bool map;
  } flags;
  uaddr bus;
  uaddr virt;
  i32 refct;
  u32 handle;
  u32 alloc_sz;
  u32 data_sz;
  u32 used_sz;
} gpu_mem;

// Constants
static const struct {
  uaddr addr_mask;
  uaddr timeout_ms;
} C = {
  .addr_mask  = ~0xc0000000,
  .timeout_ms = 10 * 1000,
};

// Globals
static struct {
  u32 page_sz;
  u32 timeout_ms;
  u32 ntasks;
  gpu_mem mem;
  struct {
    gpu_file unif;
    gpu_file rbuf;
    gpu_buf wbuf;
  } glob;
  struct {
    gpu_file inst;
    gpu_file unif;
    gpu_file rbuf;
    gpu_buf wbuf;
  } task[MAX_TASKS];
} G;

static void
print_time(struct timespec *a, struct timespec *b) {
  struct timespec diff;
  timespecsub(&diff, a, b);
  dprintf(STDERR_FILENO,
          "Execution time (sec): %ld.%.6ld\n",
          diff.tv_sec,
          diff.tv_nsec / 1000);
}

static void
print_mem(void *p, u32 sz) {
  ssize_t ret = write(STDOUT_FILENO, p, sz);
  if (ret == -1) {
    ERROR("%s", strerror(errno));
  } else if (ret != sz) {
    ERROR("Not all bytes written: %u/%u", ret, sz);
  }
}

static void
dump_all() {
  print_mem((void *)G.mem.virt, G.mem.data_sz);
}

static void
dump_wbufs() {
  if (G.glob.wbuf.active) {
    void *p = (void *)(G.mem.virt + G.glob.wbuf.offset);
    print_mem(p, G.glob.wbuf.size);
  }
  for (u32 i = 0; i < G.ntasks; ++i) {
    if (G.task[i].wbuf.active) {
      void *p = (void *)(G.mem.virt + G.task[i].wbuf.offset);
      print_mem(p, G.task[i].wbuf.size);
    }
  }
}

static result
mem_free(gpu_mem *mem) {
  result r;

  assert(mem->refct > 0);

  if (--mem->refct > 0) {
    return SUCCESS;
  }

  if (mem->flags.map) {
    r = mem_unmap(mem->virt, mem->alloc_sz);
    if (r != SUCCESS) {
      return FAILURE;
    }
    mem->flags.map = false;
  }

  if (mem->flags.lock) {
    r = mbox_unlock(mem->handle);
    if (r != SUCCESS) {
      return FAILURE;
    }
    mem->flags.lock = false;
  }

  if (mem->flags.alloc) {
    r = mbox_free(mem->handle);
    if (r != SUCCESS) {
      return FAILURE;
    }
    mem->flags.alloc = false;
  }

  return SUCCESS;
}

static result
mem_alloc(gpu_mem *mem, u32 size) {
  result r;

  assert(!mem->flags.alloc && !mem->flags.lock && !mem->flags.map);
  memset(mem, 0, sizeof(gpu_mem));

  mem->refct    = 1;
  mem->alloc_sz = ROUNDUP(size, G.page_sz);
  mem->data_sz  = size;

  r = mbox_alloc(&mem->handle, mem->alloc_sz, G.page_sz);
  if (r != SUCCESS) {
    goto error;
  } else {
    mem->flags.alloc = true;
  }

  r = mbox_lock(&mem->bus, mem->handle);
  if (r != SUCCESS) {
    goto error;
  } else {
    mem->flags.lock = true;
  }

  r = mem_map(&mem->virt, (mem->bus & C.addr_mask), mem->alloc_sz);
  if (r != SUCCESS) {
    goto error;
  } else {
    mem->flags.map = true;
  }

  return SUCCESS;

error:
  r = mem_free(mem);
  if (r != SUCCESS) {
    ERROR("Failed to free GPU memory");
  }

  return FAILURE;
}

static u32
mem_size(void) {
  u32 total = 0;

  total += sizeof(control);

  if (G.glob.unif.active) {
    total += G.glob.unif.size;
  }
  if (G.glob.rbuf.active) {
    total += G.glob.rbuf.size;
  }
  if (G.glob.wbuf.active) {
    total += G.glob.wbuf.size;
  }

  for (u32 i = 0; i < G.ntasks; ++i) {
    if (G.task[i].inst.active) {
      total += G.task[i].inst.size;
    }
    if (G.task[i].unif.active) {
      total += G.task[i].unif.size;
    }
    if (G.task[i].rbuf.active) {
      total += G.task[i].rbuf.size;
    }
    if (G.task[i].wbuf.active) {
      total += G.task[i].wbuf.size;
    }
  }

  return total;
}

static result
open_file(const char *file, u32 factor, u32 *fd, u32 *size) {
  struct stat stat;
  int ret;

  int fd0 = open(file, O_RDONLY);
  if (fd0 == -1) {
    NOTICE("%s: '%s'", strerror(errno), file);
    return FAILURE;
  }

  ret = fstat(fd0, &stat);
  if (ret == -1) {
    ERROR("%s", strerror(errno));
    goto error;
  }

  if (!S_ISREG(stat.st_mode)) {
    NOTICE("Not a regular file: '%s'", file);
    goto error;
  }

  if (stat.st_size <= 0) {
    NOTICE("Length is zero: '%s'", file);
    goto error;
  }

  if (stat.st_size % factor != 0) {
    NOTICE("Length not a factor of %u: '%s'", factor, file);
    goto error;
  }

  *fd   = fd0;
  *size = stat.st_size;

  return SUCCESS;

error:
  ret = close(fd0);
  if (ret == -1) {
    ERROR("%s", strerror(errno));
  }

  return FAILURE;
}

static result
dup_file(gpu_file *dst, const gpu_file *src) {
  assert(src->active && !dst->active);

  int ret = dup(src->fd);
  if (ret == -1) {
    ERROR("%s", strerror(errno));
    return FAILURE;
  }

  *dst    = *src;
  dst->fd = ret;

  return SUCCESS;
}

static result
dup_buf(gpu_buf *dst, const gpu_buf *src) {
  assert(src->active && !dst->active);

  *dst = *src;

  return SUCCESS;
}

static result
init_file(gpu_mem *mem, gpu_file *file) {
  int ret;

  assert((mem->data_sz - mem->used_sz) >= file->size);

  ret = lseek(file->fd, 0, SEEK_SET);
  if (ret == -1) {
    ERROR("%s", strerror(errno));
    return FAILURE;
  }

  ret = read(file->fd, (void *)(mem->virt + mem->used_sz), file->size);
  if (ret == -1) {
    ERROR("%s", strerror(errno));
    return FAILURE;
  }

  file->offset = mem->used_sz;
  mem->used_sz += file->size;

  return SUCCESS;
}

static result
init_buf(gpu_mem *mem, gpu_buf *buf) {
  assert((mem->data_sz - mem->used_sz) >= buf->size);

  buf->offset = mem->used_sz;
  mem->used_sz += buf->size;

  return SUCCESS;
}

static result
init_mem(void) {
  control cntl = {{{0}}};
  result r;

  u32 size = mem_size();

  r = mem_alloc(&G.mem, size);
  if (r != SUCCESS) {
    return FAILURE;
  }

  G.mem.used_sz += sizeof(cntl);

  // Instructions

  for (u32 i = 0; i < G.ntasks; ++i) {
    if (G.task[i].inst.active) {
      r = init_file(&G.mem, &G.task[i].inst);
      if (r != SUCCESS) {
        return FAILURE;
      }
    }
  }

  // Uniforms

  if (G.glob.unif.active) {
    r = init_file(&G.mem, &G.glob.unif);
    if (r != SUCCESS) {
      return FAILURE;
    }
  }

  for (u32 i = 0; i < G.ntasks; ++i) {
    if (G.task[i].unif.active) {
      r = init_file(&G.mem, &G.task[i].unif);
      if (r != SUCCESS) {
        return FAILURE;
      }
    }
  }

  // Read Buffers

  if (G.glob.rbuf.active) {
    r = init_file(&G.mem, &G.glob.rbuf);
    if (r != SUCCESS) {
      return FAILURE;
    }
  }

  for (u32 i = 0; i < G.ntasks; ++i) {
    if (G.task[i].rbuf.active) {
      r = init_file(&G.mem, &G.task[i].rbuf);
      if (r != SUCCESS) {
        return FAILURE;
      }
    }
  }

  // Write Buffers

  if (G.glob.wbuf.active) {
    r = init_buf(&G.mem, &G.glob.wbuf);
    if (r != SUCCESS) {
      return FAILURE;
    }
  }

  for (u32 i = 0; i < G.ntasks; ++i) {
    if (G.task[i].wbuf.active) {
      r = init_buf(&G.mem, &G.task[i].wbuf);
      if (r != SUCCESS) {
        return FAILURE;
      }
    }
  }

  // Control Buffer (Write)

  for (u32 i = 0; i < G.ntasks; ++i) {
    if (G.task[i].unif.active) {
      cntl.task[i].unif = G.mem.bus + G.task[i].unif.offset;
    } else if (G.glob.unif.active) {
      cntl.task[i].unif = G.mem.bus + G.glob.unif.offset;
    }
    if (G.task[i].inst.active) {
      cntl.task[i].inst = G.mem.bus + G.task[i].inst.offset;
    }
  }

  memcpy((void *)G.mem.virt, (void *)&cntl, sizeof(cntl));

  if (G.mem.used_sz != G.mem.data_sz) {
    ERROR("Failed to initialize memory");
    return FAILURE;
  }

  return SUCCESS;
}

static bool
is_link_inst(u32 w) {
  enum {
    LDI_R0   = 0xe0020827,
    LDI_R1   = 0xe0020867,
    LDI_R2   = 0xe00208a7,
    LDI_R3   = 0xe00208e7,
    LDI_VPMR = 0xe0020ca7,
    LDI_VPMW = 0xe0021ca7,
    LDI_T0S  = 0xe0020e27,
    LDI_T1S  = 0xe0020f27,
  };
  switch (w) {
  case LDI_R0:
  case LDI_R1:
  case LDI_R2:
  case LDI_R3:
  case LDI_VPMR:
  case LDI_VPMW:
  case LDI_T0S:
  case LDI_T1S:
    return true;
  default:
    return false;
  }
}

static result
link_mem(void) {
  enum {
    GLOB_RBUF = 0xfffffff1,
    GLOB_WBUF = 0xfffffff2,
    TASK_RBUF = 0xfffffffa,
    TASK_WBUF = 0xfffffffb,
  };

  bool error = false;

  for (u32 i = 0; i < G.ntasks; ++i) {
    bool glob_rbuf = false;
    bool glob_wbuf = false;
    bool task_rbuf = false;
    bool task_wbuf = false;

    assert(G.task[i].inst.active);

    u32 nwords = G.task[i].inst.size / 4;
    u32 *p     = (u32 *)(G.mem.virt + G.task[i].inst.offset);

    // Check all 64-bit, little-endian instructions
    for (u32 j = 0; j < (nwords - 1); j += 2) {
      if (is_link_inst(p[j + 1])) {
        switch (p[j]) {
        case GLOB_RBUF:
          p[j]      = G.mem.bus + G.glob.rbuf.offset;
          glob_rbuf = true;
          break;
        case GLOB_WBUF:
          p[j]      = G.mem.bus + G.glob.wbuf.offset;
          glob_wbuf = true;
          break;
        case TASK_RBUF:
          p[j]      = G.mem.bus + G.task[i].rbuf.offset;
          task_rbuf = true;
          break;
        case TASK_WBUF:
          p[j]      = G.mem.bus + G.task[i].wbuf.offset;
          task_wbuf = true;
          break;
        }
      }
    }

    if (glob_rbuf && !G.glob.rbuf.active) {
      NOTICE("Missing global read buffer for task %u placeholder", i);
      error = true;
    }

    if (glob_wbuf && !G.glob.wbuf.active) {
      NOTICE("Missing global write buffer for task %u placeholder", i);
      error = true;
    }

    if (task_rbuf && !G.task[i].rbuf.active) {
      NOTICE("Missing read buffer for task %u placeholder", i);
      error = true;
    } else if (!task_rbuf && G.task[i].rbuf.active) {
      NOTICE("Missing placeholder for task %u read buffer", i);
      error = true;
    }

    if (task_wbuf && !G.task[i].wbuf.active) {
      NOTICE("Missing write buffer for task %u placeholder", i);
      error = true;
    } else if (!task_wbuf && G.task[i].wbuf.active) {
      NOTICE("Missing placeholder for task %u write buffer", i);
      error = true;
    }
  }

  return error ? FAILURE : SUCCESS;
}

result
gpu_exec_via_mbox(opt o) {
  const u32 timeout = G.timeout_ms > 0 ? G.timeout_ms : C.timeout_ms;
  bool error        = false;
  struct timespec time[2];
  result r;

  o.executing = true;

  if (G.ntasks == 0) {
    return SUCCESS;
  }

  r = init_mem();
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = link_mem();
  if (r != SUCCESS) {
    return FAILURE;
  }

  if (!o.isatty && o.dump0) {
    dump_all();
  }

  if (o.dry) {
    return SUCCESS;
  }

  r = mbox_enable(o);
  if (r != SUCCESS) {
    return FAILURE;
  }

  if (o.mdebug) {
    reg_debug_before();
  }

  if (o.mctr0) {
    reg_init_pctr();
  }

  if (o.mctr0 || o.mctr1) {
    reg_perf_before();
  }

  if (o.mtime) {
    clock_gettime(CLOCK_MONOTONIC_RAW, &time[0]);
  }

  r = mbox_exec_qpu(G.ntasks, G.mem.bus, false, timeout);
  if (r != SUCCESS) {
    ERROR("Failed to execute GPU program");
    error = true;
  }

  if (o.mtime) {
    clock_gettime(CLOCK_MONOTONIC_RAW, &time[1]);
  }

  if (o.mctr0 || o.mctr1) {
    reg_perf_after();
  }

  if (o.mdebug) {
    reg_debug_after();
  }

  if (!o.isatty && o.dump1) {
    dump_all();
  }

  if (!o.isatty && !o.dump0 && !o.dump1) {
    dump_wbufs();
  }

  if (o.mdebug) {
    reg_debug_print(o);
  }

  if (o.mctr0 || o.mctr1) {
    reg_perf_print(o);
  }

  if (o.mtime) {
    print_time(&time[0], &time[1]);
  }

  r = mbox_disable(o);
  if (r != SUCCESS) {
    error = true;
  }

  return error ? FAILURE : SUCCESS;
}

result
gpu_replicate(u32 mult) {
  result r;

  if ((mult <= 1) || (G.ntasks == 0)) {
    NOTICE("Nothing to replicate");
    return FAILURE;
  }

  if ((mult * G.ntasks) > MAX_TASKS) {
    NOTICE("Max GPU tasks exceeded");
    return FAILURE;
  }

  for (u32 dst = G.ntasks; dst < (mult * G.ntasks); ++dst) {
    u32 src = dst % G.ntasks;

    if (G.task[src].inst.active) {
      r = dup_file(&G.task[dst].inst, &G.task[src].inst);
      if (r != SUCCESS) {
        return FAILURE;
      }
    }

    if (G.task[src].unif.active) {
      r = dup_file(&G.task[dst].unif, &G.task[src].unif);
      if (r != SUCCESS) {
        return FAILURE;
      }
    }

    if (G.task[src].rbuf.active) {
      r = dup_file(&G.task[dst].rbuf, &G.task[src].rbuf);
      if (r != SUCCESS) {
        return FAILURE;
      }
    }

    if (G.task[src].wbuf.active) {
      r = dup_buf(&G.task[dst].wbuf, &G.task[src].wbuf);
      if (r != SUCCESS) {
        return FAILURE;
      }
    }
  }

  G.ntasks *= mult;

  return SUCCESS;
}

result
gpu_task_wbuf(u32 size) {
  if (G.ntasks > MAX_TASKS) {
    NOTICE("Max GPU tasks exceeded");
    return FAILURE;
  }

  if (G.task[G.ntasks - 1].wbuf.active) {
    NOTICE("Duplicate task write buffer: '%u'", size);
    return FAILURE;
  }

  G.task[G.ntasks - 1].wbuf.size   = size;
  G.task[G.ntasks - 1].wbuf.active = true;

  return SUCCESS;
}

result
gpu_task_rbuf(const char *file) {
  u32 fd;
  u32 size;

  if (G.ntasks > MAX_TASKS) {
    NOTICE("Max GPU tasks exceeded");
    return FAILURE;
  }

  if (G.task[G.ntasks - 1].rbuf.active) {
    NOTICE("Duplicate task read buffer: '%s'", file);
    return FAILURE;
  }

  result r = open_file(file, 4, &fd, &size);
  if (r != SUCCESS) {
    return FAILURE;
  }

  G.task[G.ntasks - 1].rbuf.fd     = fd;
  G.task[G.ntasks - 1].rbuf.size   = size;
  G.task[G.ntasks - 1].rbuf.active = true;

  return SUCCESS;
}

result
gpu_task_unif(const char *file) {
  u32 fd;
  u32 size;

  if (G.ntasks > MAX_TASKS) {
    NOTICE("Max GPU tasks exceeded");
    return FAILURE;
  }

  if (G.task[G.ntasks - 1].unif.active) {
    NOTICE("Duplicate task uniforms: '%s'", file);
    return FAILURE;
  }

  result r = open_file(file, 4, &fd, &size);
  if (r != SUCCESS) {
    return FAILURE;
  }

  G.task[G.ntasks - 1].unif.fd     = fd;
  G.task[G.ntasks - 1].unif.size   = size;
  G.task[G.ntasks - 1].unif.active = true;

  return SUCCESS;
}

result
gpu_task_inst(const char *file) {
  u32 fd;
  u32 size;

  if (G.ntasks > MAX_TASKS) {
    NOTICE("Max GPU tasks exceeded");
    return FAILURE;
  }
  if (G.task[G.ntasks - 1].inst.active) {
    NOTICE("Duplicate task instructions: '%s'", file);
    return FAILURE;
  }

  result r = open_file(file, 8, &fd, &size);
  if (r != SUCCESS) {
    return FAILURE;
  }

  G.task[G.ntasks - 1].inst.fd     = fd;
  G.task[G.ntasks - 1].inst.size   = size;
  G.task[G.ntasks - 1].inst.active = true;

  return SUCCESS;
}

result
gpu_next_task(void) {
  if (G.ntasks >= MAX_TASKS) {
    NOTICE("Max GPU tasks exceeded");
    return FAILURE;
  }

  ++G.ntasks;

  return SUCCESS;
}

result
gpu_glob_wbuf(u32 size) {
  if (G.glob.wbuf.active) {
    NOTICE("Duplicate global write buffer: '%u'", size);
    return FAILURE;
  }

  G.glob.wbuf.size   = size;
  G.glob.wbuf.active = true;

  return SUCCESS;
}

result
gpu_glob_rbuf(const char *file) {
  u32 fd;
  u32 size;

  if (G.glob.rbuf.active) {
    NOTICE("Duplicate global read buffer: '%s'", file);
    return FAILURE;
  }

  result r = open_file(file, 4, &fd, &size);
  if (r != SUCCESS) {
    return FAILURE;
  }

  G.glob.rbuf.fd     = fd;
  G.glob.rbuf.size   = size;
  G.glob.rbuf.active = true;

  return SUCCESS;
}

result
gpu_glob_unif(const char *file) {
  u32 fd;
  u32 size;

  if (G.glob.unif.active) {
    NOTICE("Duplicate global uniforms: '%s'", file);
    return FAILURE;
  }

  result r = open_file(file, 4, &fd, &size);
  if (r != SUCCESS) {
    return FAILURE;
  }

  G.glob.unif.fd     = fd;
  G.glob.unif.size   = size;
  G.glob.unif.active = true;

  return SUCCESS;
}

void
gpu_set_timeout(u32 nsec) {
  G.timeout_ms = nsec * 1000;
}

bool
gpu_has_task(void) {
  return G.ntasks > 0;
}

result
gpu_cleanup(void) {
  bool error = false;
  result r;
  int ret;

  if (G.glob.unif.active) {
    ret = close(G.glob.unif.fd);
    if (ret == -1) {
      ERROR("%s", strerror(errno));
      error = true;
    }
    G.glob.unif.active = false;
  }

  if (G.glob.rbuf.active) {
    ret = close(G.glob.rbuf.fd);
    if (ret == -1) {
      ERROR("%s", strerror(errno));
      error = true;
    }
    G.glob.rbuf.active = false;
  }

  for (int i = 0; i < MAX_TASKS; ++i) {
    if (G.task[i].inst.active) {
      ret = close(G.task[i].inst.fd);
      if (ret == -1) {
        ERROR("%s", strerror(errno));
        error = true;
      }
      G.task[i].inst.active = false;
    }

    if (G.task[i].unif.active) {
      ret = close(G.task[i].unif.fd);
      if (ret == -1) {
        ERROR("%s", strerror(errno));
        error = true;
      }
      G.task[i].unif.active = false;
    }

    if (G.task[i].rbuf.active) {
      ret = close(G.task[i].rbuf.fd);
      if (ret == -1) {
        ERROR("%s", strerror(errno));
        error = true;
      }
      G.task[i].rbuf.active = false;
    }
  }

  if (G.mem.refct > 0) {
    r = mem_free(&G.mem);
    if (r != SUCCESS) {
      ERROR("Failed to free GPU memory");
      error = true;
    }
  }

  return error ? FAILURE : SUCCESS;
};

result
gpu_init(void) {
  errno = 0;

  long ret = sysconf(_SC_PAGE_SIZE);
  if (ret == -1) {
    if (errno == 0) {
      ERROR("Option not supported: Page size");
    } else {
      ERROR("%s", strerror(errno));
    }
    return FAILURE;
  }

  G.page_sz = (u32)ret;

  return SUCCESS;
}
