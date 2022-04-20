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
#include "reg.h"
#include "types.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HANDLE_EXECUTE(x)                  \
  if (strcmp(argv[optind], #x) == 0) {     \
    result r = handle_##x(argv[++optind]); \
    if (r != SUCCESS) {                    \
      return FAILURE;                      \
    }                                      \
    continue;                              \
  }

#define HANDLE_REGISTER(x)                        \
  if (strcmp(argv[optind], #x) == 0) {            \
    i64 arg;                                      \
    result r = parse_num(&arg, argv[optind + 1]); \
    if (r != SUCCESS) {                           \
      reg_read_##x(G.opt);                        \
    } else {                                      \
      reg_write_##x((u32)arg);                    \
      ++optind;                                   \
    }                                             \
    continue;                                     \
  }

#define HANDLE_FIRMWARE(x)             \
  if (strcmp(argv[optind], #x) == 0) { \
    result r = mbox_##x(G.opt);        \
    if (r != SUCCESS) {                \
      return FAILURE;                  \
    }                                  \
    continue;                          \
  }

// Globals
static struct {
  bool help;
  opt opt;
} G;

static void
print_help(void) {
  const char *s =
    "Usage: qpu [options] <command> [arguments]                             \n"
    "OPTIONS                                                                \n"
    "  Help                                                                 \n"
    "    -h            Print This Help Message                              \n"
    "    -p            Print Perf Counter Help                              \n"
    "    -r            Print Register Help                                  \n"
    "  Measure                                                              \n"
    "    -1            Monitor Preconfigured Perf Counters                  \n"
    "    -2            Monitor User-Configured Perf Counters                \n"
    "    -d            Monitor Debug Registers                              \n"
    "    -t            Measure Execution Time                               \n"
    "  Other                                                                \n"
    "    -a            Dump GPU Memory After Execution                      \n"
    "    -b            Dump GPU Memory Before Execution                     \n"
    "    -g <sec>      Set GPU Timeout                                      \n"
    "    -n            Dry Run                                              \n"
    "    -v            Verbose Output                                       \n"
    "COMMANDS                                                               \n"
    "  firmware                                                             \n"
    "    enable        Enable the GPU                                       \n"
    "    disable       Disable the GPU                                      \n"
    "    board         Print Board Version                                  \n"
    "    clocks        Print Clock States and Rates                         \n"
    "    memory        Print ARM/GPU Memory Split                           \n"
    "    power         Print Power States                                   \n"
    "    temp          Print Current Temperature                            \n"
    "    version       Print Firmware Version                               \n"
    "    voltage       Print Current Voltages                               \n"
    "  register                                                             \n"
    "    <name>        Read Register                                        \n"
    "    <name> <num>  Write Register                                       \n"
    "  execute                                                              \n"
    "    i <file>      Add Instructions                                     \n"
    "    u <file>      Add Uniforms                                         \n"
    "    r <file>      Add Read Buffer                                      \n"
    "    w <size>      Add Write Buffer                                     \n"
    "    x <mult>      Replicate Preceeding Tasks                           ";
  LOG("%s", s);
}

static result
parse_num(i64 *i, const char *x) {
  char *e;

  if (x == NULL || *x == '\0') {
    return FAILURE;
  }

  *i = strtoll(x, &e, 0);
  if (*e != '\0') {
    return FAILURE;
  }

  return SUCCESS;
}

static result
parse_timeout(u32 *timeout, const char *num) {
  result r;
  i64 nsec;

  r = parse_num(&nsec, num);
  if (r != SUCCESS) {
    NOTICE("Invalid number '%s'", num);
    return FAILURE;
  }

  if (nsec <= 0) {
    NOTICE("Invalid timeout '%s'", num);
    return FAILURE;
  }

  *timeout = nsec;

  return SUCCESS;
}

static result
handle_x(const char *num) {
  result r;
  i64 mult;

  if (!gpu_has_task()) {
    NOTICE("No GPU tasks");
    return FAILURE;
  }

  r = parse_num(&mult, num);
  if (r != SUCCESS) {
    NOTICE("Invalid number '%s'", num);
    return FAILURE;
  }

  if (mult <= 1) {
    NOTICE("Invalid multiplier '%s'", num);
    return FAILURE;
  }

  r = gpu_replicate(mult);
  if (r != SUCCESS) {
    return FAILURE;
  }

  return SUCCESS;
}

static result
handle_w(const char *num) {
  result r;
  i64 size;

  if (!num) {
    NOTICE("Missing buffer size");
    return FAILURE;
  }

  r = parse_num(&size, num);
  if (r != SUCCESS) {
    NOTICE("Invalid number '%s'", num);
    return FAILURE;
  }

  if (size <= 0) {
    NOTICE("Invalid write buffer size '%s'", num);
    return FAILURE;
  }

  if (gpu_has_task()) {
    r = gpu_task_wbuf(size);
    if (r != SUCCESS) {
      return FAILURE;
    }
  } else {
    r = gpu_glob_wbuf(size);
    if (r != SUCCESS) {
      return FAILURE;
    }
  }

  return SUCCESS;
}

static result
handle_r(const char *file) {
  result r;

  if (!file) {
    NOTICE("Missing filename");
    return FAILURE;
  }

  if (gpu_has_task()) {
    r = gpu_task_rbuf(file);
    if (r != SUCCESS) {
      return FAILURE;
    }
  } else {
    r = gpu_glob_rbuf(file);
    if (r != SUCCESS) {
      return FAILURE;
    }
  }

  return SUCCESS;
}

static result
handle_u(const char *file) {
  result r;

  if (!file) {
    NOTICE("Missing filename");
    return FAILURE;
  }

  if (gpu_has_task()) {
    r = gpu_task_unif(file);
    if (r != SUCCESS) {
      return FAILURE;
    }
  } else {
    r = gpu_glob_unif(file);
    if (r != SUCCESS) {
      return FAILURE;
    }
  }

  return SUCCESS;
}

static result
handle_i(const char *file) {
  result r;

  if (!file) {
    NOTICE("Missing filename");
    return FAILURE;
  }

  r = gpu_next_task();
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = gpu_task_inst(file);
  if (r != SUCCESS) {
    return FAILURE;
  }

  return SUCCESS;
}

static result
command_execute(int argc, char **argv) {
  if (argv[optind + 1] == NULL) {
    NOTICE("Missing argument(s)");
    return FAILURE;
  }

  while (++optind < argc) {
    HANDLE_EXECUTE(i);
    HANDLE_EXECUTE(u);
    HANDLE_EXECUTE(r);
    HANDLE_EXECUTE(w);
    HANDLE_EXECUTE(x);
    if (argv[optind] != NULL) {
      NOTICE("Unsupported argument '%s'", argv[optind]);
      return FAILURE;
    }
  }

  return SUCCESS;
}

static result
command_register(int argc, char **argv) {
  if (!reg_gpu_is_enabled()) {
    NOTICE("GPU disabled");
    return FAILURE;
  }

  if (argv[optind + 1] == NULL) {
    NOTICE("Missing argument(s)");
    return FAILURE;
  }

  while (++optind < argc) {
    HANDLE_REGISTER(ident0);
    HANDLE_REGISTER(ident1);
    HANDLE_REGISTER(ident2);
    HANDLE_REGISTER(scratch);
    HANDLE_REGISTER(l2cactl);
    HANDLE_REGISTER(slcactl);
    HANDLE_REGISTER(intctl);
    HANDLE_REGISTER(intena);
    HANDLE_REGISTER(intdis);
    HANDLE_REGISTER(ct0cs);
    HANDLE_REGISTER(ct1cs);
    HANDLE_REGISTER(ct0ea);
    HANDLE_REGISTER(ct1ea);
    HANDLE_REGISTER(ct0ca);
    HANDLE_REGISTER(ct1ca);
    HANDLE_REGISTER(ct00ra0);
    HANDLE_REGISTER(ct01ra0);
    HANDLE_REGISTER(ct0lc);
    HANDLE_REGISTER(ct1lc);
    HANDLE_REGISTER(ct0pc);
    HANDLE_REGISTER(ct1pc);
    HANDLE_REGISTER(pcs);
    HANDLE_REGISTER(bfc);
    HANDLE_REGISTER(rfc);
    HANDLE_REGISTER(bpca);
    HANDLE_REGISTER(bpcs);
    HANDLE_REGISTER(bpoa);
    HANDLE_REGISTER(bpos);
    HANDLE_REGISTER(bxcf);
    HANDLE_REGISTER(sqrsv0);
    HANDLE_REGISTER(sqrsv1);
    HANDLE_REGISTER(sqcntl);
    HANDLE_REGISTER(srqpc);
    HANDLE_REGISTER(srqua);
    HANDLE_REGISTER(srqul);
    HANDLE_REGISTER(srqcs);
    HANDLE_REGISTER(vpacntl);
    HANDLE_REGISTER(vpmbase);
    HANDLE_REGISTER(pctrc);
    HANDLE_REGISTER(pctre);
    HANDLE_REGISTER(pctr);
    HANDLE_REGISTER(pctr0);
    HANDLE_REGISTER(pctr1);
    HANDLE_REGISTER(pctr2);
    HANDLE_REGISTER(pctr3);
    HANDLE_REGISTER(pctr4);
    HANDLE_REGISTER(pctr5);
    HANDLE_REGISTER(pctr6);
    HANDLE_REGISTER(pctr7);
    HANDLE_REGISTER(pctr8);
    HANDLE_REGISTER(pctr9);
    HANDLE_REGISTER(pctr10);
    HANDLE_REGISTER(pctr11);
    HANDLE_REGISTER(pctr12);
    HANDLE_REGISTER(pctr13);
    HANDLE_REGISTER(pctr14);
    HANDLE_REGISTER(pctr15);
    HANDLE_REGISTER(pctrs);
    HANDLE_REGISTER(pctrs0);
    HANDLE_REGISTER(pctrs1);
    HANDLE_REGISTER(pctrs2);
    HANDLE_REGISTER(pctrs3);
    HANDLE_REGISTER(pctrs4);
    HANDLE_REGISTER(pctrs5);
    HANDLE_REGISTER(pctrs6);
    HANDLE_REGISTER(pctrs7);
    HANDLE_REGISTER(pctrs8);
    HANDLE_REGISTER(pctrs9);
    HANDLE_REGISTER(pctrs10);
    HANDLE_REGISTER(pctrs11);
    HANDLE_REGISTER(pctrs12);
    HANDLE_REGISTER(pctrs13);
    HANDLE_REGISTER(pctrs14);
    HANDLE_REGISTER(pctrs15);
    HANDLE_REGISTER(dbqite);
    HANDLE_REGISTER(dbqitc);
    HANDLE_REGISTER(dbge);
    HANDLE_REGISTER(fdbgo);
    HANDLE_REGISTER(fdbgb);
    HANDLE_REGISTER(fdbgr);
    HANDLE_REGISTER(fdbgs);
    HANDLE_REGISTER(errstat);
    if (argv[optind] != NULL) {
      NOTICE("Unsupported register '%s'", argv[optind]);
      return FAILURE;
    }
  }

  return SUCCESS;
}

static result
command_firmware(int argc, char **argv) {
  if (argv[optind + 1] == NULL) {
    NOTICE("Missing argument(s)");
    return FAILURE;
  }

  while (++optind < argc) {
    HANDLE_FIRMWARE(enable);
    HANDLE_FIRMWARE(disable);
    HANDLE_FIRMWARE(board);
    HANDLE_FIRMWARE(clocks);
    HANDLE_FIRMWARE(memory);
    HANDLE_FIRMWARE(power);
    HANDLE_FIRMWARE(temp);
    HANDLE_FIRMWARE(version);
    HANDLE_FIRMWARE(voltage);
    if (argv[optind] != NULL) {
      NOTICE("Unsupported command '%s'", argv[optind]);
      return FAILURE;
    }
  }

  return SUCCESS;
}

static result
parse_command(int argc, char **argv) {
  if (argv[optind] == NULL) {
    NOTICE("Missing command");
    return FAILURE;
  }

  if (strcmp(argv[optind], "firmware") == 0) {
    return command_firmware(argc, argv);
  }
  if (strcmp(argv[optind], "register") == 0) {
    return command_register(argc, argv);
  }
  if (strcmp(argv[optind], "execute") == 0) {
    return command_execute(argc, argv);
  }

  if (argv[optind] != NULL) {
    NOTICE("Invalid command '%s'", argv[optind]);
    return FAILURE;
  } else {
    ERROR("Command parse failed");
    return FAILURE;
  }
}

static result
parse_options(int argc, char **argv) {
  if (isatty(STDOUT_FILENO)) {
    G.opt.isatty = true;
  }

  if (argc == 1) {
    print_help();
    G.help = true;
    return SUCCESS;
  }

  while (true) {
    int c = getopt(argc, argv, ":hpr12dtabg:nv");
    if (c == -1) {
      break;
    }

    switch (c) {
    case 'h':
      print_help();
      G.help = true;
      break;
    case 'p':
      reg_print_perf();
      G.help = true;
      break;
    case 'r':
      reg_print_reg();
      G.help = true;
      break;
    case '1':
      G.opt.mctr0 = true;
      break;
    case '2':
      G.opt.mctr1 = true;
      break;
    case 'd':
      G.opt.mdebug = true;
      break;
    case 't':
      G.opt.mtime = true;
      break;
    case 'a':
      G.opt.dump1 = true;
      break;
    case 'b':
      G.opt.dump0 = true;
      break;
    case 'g': {
      result r = parse_timeout(&G.opt.timeout_s, optarg);
      if (r != SUCCESS) {
        return FAILURE;
      }
    } break;
    case 'n':
      G.opt.dry = true;
      break;
    case 'v':
      G.opt.verbose = true;
      break;
    case ':':
      NOTICE("Missing argument for '%s'", argv[optind - 1]);
      return FAILURE;
    case '?':
      NOTICE("Invalid option '%s'", argv[optind]);
      return FAILURE;
    default:
      ERROR("Unexpected: '%s'", argv[optind - 1]);
      return FAILURE;
    }
  }

  if (G.opt.mctr0 && G.opt.mctr1) {
    NOTICE("Conflicting options: -1, -2");
    return FAILURE;
  }

  return SUCCESS;
}

int
main(int argc, char **argv) {
  bool error = false;
  result r;

  r = parse_options(argc, argv);
  if (r != SUCCESS) {
    return EXIT_FAILURE;
  }

  if (G.help) {
    return EXIT_SUCCESS;
  }

  r = mbox_init();
  if (r != SUCCESS) {
    error = true;
    goto out;
  }

  r = reg_init();
  if (r != SUCCESS) {
    error = true;
    goto out;
  }

  r = gpu_init();
  if (r != SUCCESS) {
    error = true;
    goto out;
  }

  r = parse_command(argc, argv);
  if (r != SUCCESS) {
    error = true;
    goto out;
  }

  r = gpu_exec_via_mbox(G.opt);
  if (r != SUCCESS) {
    error = true;
    goto out;
  }

out:
  r = gpu_cleanup();
  if (r != SUCCESS) {
    error = true;
  }

  r = reg_cleanup();
  if (r != SUCCESS) {
    error = true;
  }

  r = mbox_cleanup();
  if (r != SUCCESS) {
    error = true;
  }

  return error ? EXIT_FAILURE : EXIT_SUCCESS;
}
