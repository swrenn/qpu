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

#include "mbox.h"

#include "log.h"
#include "types.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

// What Is Mailbox and How Does it Work?
//
// (Citations from https://github.com/raspberrypi/linux)
//
// "Mailbox is a framework to control hardware communication between on-chip
// processors through queued messages and interrupt driven signals."
//   -- drivers/mailbox/Kconfig
//
// The following device tree file identifies the mailbox that facilitates
// communication between the ARM CPU and VideoCore IV GPU on the Raspberry Pi,
// along with its bus address, 0x7e00b840.
//   -- arch/arm/boot/dts/bcm2708-rpi.dtsi
//
// While booting, the Linux kernel executes bcm2835_mbox_probe(). This
// subroutine initializes and registers the mailbox. Initialization includes
// mapping the bus address of the mailbox to a physical address. According to
// Broadcom's documentation, this mapping should be from 0x7e00b840 to
// 0x2000b840, but a kernel boot message prints the address 0x2000b880.
//   -- drivers/mailbox/bcm2835-mailbox.c
//
// Still booting, the kernel executes rpi_firmware_probe(). This initializes and
// registers a driver as the client for the new mailbox. This driver will be the
// interface to the Raspberry Pi firmware running on the VideoCore IV GPU.
//   -- drivers/firmware/raspberrypi.c
//
// Finally, the kernel executes vcio_init(). This creates the character device
// /dev/vcio, which provides user space access to the above driver. The
// subroutine dynamically allocates the major number for the device, though it
// typically chooses 248. The subroutine also saves a pointer to the above
// driver and registers vcio_device_ioctl() to handle unlocked ioctl commands to
// the device. We use those commands below.
//   -- drivers/char/broadcom/vcio.c
//
// All magic numbers are taken from these files:
//   -- drivers/char/broadcom/vcio.c
//   -- include/soc/bcm2835/raspberrypi-firmware.h
//
// This wiki describes the mailbox interface:
//   -- https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface

enum {
  FW_SUCCESS = 0,
  FW_TIMEOUT = 1 << 31,
};

enum {
  STATUS_REQUEST = 0,
  STATUS_SUCCESS = 0x80000000,
  STATUS_ERROR   = 0x80000001,
};

enum {
  TAG_PROPERTY_END    = 0,
  TAG_GET_FW_REVISION = 0x00000001,
  TAG_GET_FW_VARIANT  = 0x00000002,
  TAG_GET_BD_MODEL    = 0x00010001,
  TAG_GET_BD_REVISION = 0x00010002,
  TAG_GET_BD_MAC      = 0x00010003,
  TAG_GET_BD_SERIAL   = 0x00010004,
  TAG_GET_MEM_ARM     = 0x00010005,
  TAG_GET_MEM_VC4     = 0x00010006,
  TAG_GET_POWER_STATE = 0x00020001,
  TAG_GET_CLOCK_STATE = 0x00030001,
  TAG_GET_CLOCK_RATE  = 0x00030002,
  TAG_GET_CLOCK_MAX   = 0x00030004,
  TAG_GET_CLOCK_MIN   = 0x00030007,
  TAG_GET_CLOCK_TURBO = 0x00030009,
  TAG_GET_VOLTAGE     = 0x00030003,
  TAG_GET_VOLTMAX     = 0x00030005,
  TAG_GET_TEMP        = 0x00030006,
  TAG_GET_VOLTMIN     = 0x00030008,
  TAG_GET_TEMPMAX     = 0x0003000a,
  TAG_MEM_ALLOC       = 0x0003000c,
  TAG_MEM_LOCK        = 0x0003000d,
  TAG_MEM_UNLOCK      = 0x0003000e,
  TAG_MEM_FREE        = 0x0003000f,
  TAG_EXEC_CODE       = 0x00030010,
  TAG_EXEC_QPU        = 0x00030011,
  TAG_QPU_ENABLE      = 0x00030012,
};

enum {
  CLOCK_EMMC      = 1,
  CLOCK_UART      = 2,
  CLOCK_ARM       = 3,
  CLOCK_CORE      = 4,
  CLOCK_V3D       = 5,
  CLOCK_H264      = 6,
  CLOCK_ISP       = 7,
  CLOCK_SDRAM     = 8,
  CLOCK_PIXEL     = 9,
  CLOCK_PWM       = 10,
  CLOCK_HEVC      = 11,
  CLOCK_EMMC2     = 12,
  CLOCK_M2MC      = 13,
  CLOCK_PIXEL_BVB = 14,
};

enum {
  POWER_SD_CARD = 0,
  POWER_UART0   = 1,
  POWER_UART1   = 2,
  POWER_USB_HCD = 3,
  POWER_I2C0    = 4,
  POWER_I2C1    = 5,
  POWER_I2C2    = 6,
  POWER_SPI     = 7,
  POWER_CCP2TX  = 8,
};

enum {
  VOLT_CORE       = 1,
  VOLT_SDRAM_CORE = 2,
  VOLT_SDRAM_PHY  = 3,
  VOLT_SDRAM_IO   = 4,
};

enum {
  MEM_DIRECT     = 1 << 2,  // Bus alias 0xCxxxxxxx
  MEM_L2COHERENT = 2 << 2,  // Bus alias 0x8xxxxxxx
  MEM_L2ALLOC    = 3 << 2,  // Bus alias 0x4xxxxxxx
};

struct hdr {
  u32 msg_sz;
  u32 status;
};

// Globals
static struct { int vcio_fd; } G;

static result
do_ioctl(void *msg) {
  int ret = ioctl(G.vcio_fd, _IOWR(100, 0, char *), msg);
  if (ret == -1) {
    ERROR("%s (%d)", strerror(errno), errno);
    return FAILURE;
  } else if (((struct hdr *)msg)->status != STATUS_SUCCESS) {
    ERROR("Firmware: Unspecified error");
    return FAILURE;
  }

  return SUCCESS;
}

static result
info_voltage(u32 id, const char *name) {
  struct {
    u32 msg_sz;
    u32 status;
    u32 volt_tag;
    u32 volt_data_sz;
    u32 volt_resp_sz;
    u32 volt_data[2];
    u32 min_tag;
    u32 min_data_sz;
    u32 min_resp_sz;
    u32 min_data[2];
    u32 max_tag;
    u32 max_data_sz;
    u32 max_resp_sz;
    u32 max_data[2];
    u32 end;
  } msg = {
    .msg_sz       = sizeof(msg),
    .status       = STATUS_REQUEST,
    .volt_tag     = TAG_GET_VOLTAGE,
    .volt_data_sz = sizeof(msg.volt_data),
    .volt_data[0] = id,
    .min_tag      = TAG_GET_VOLTMIN,
    .min_data_sz  = sizeof(msg.min_data),
    .min_data[0]  = id,
    .max_tag      = TAG_GET_VOLTMAX,
    .max_data_sz  = sizeof(msg.max_data),
    .max_data[0]  = id,
    .end          = TAG_PROPERTY_END,
  };

  result r = do_ioctl(&msg);
  if (r != SUCCESS) {
    return FAILURE;
  }

  LOG("%s: %.2f V (Min %.2f V, Max %.2f V)",
      name,
      msg.volt_data[1] / 1000.0 / 1000.0,
      msg.min_data[1] / 1000.0 / 1000.0,
      msg.max_data[1] / 1000.0 / 1000.0);

  return SUCCESS;
}

static result
info_power(u32 id, const char *name) {
  struct {
    u32 msg_sz;
    u32 status;
    u32 tag;
    u32 data_sz;
    u32 resp_sz;
    u32 data[2];
    u32 end;
  } msg = {
    .msg_sz  = sizeof(msg),
    .status  = STATUS_REQUEST,
    .tag     = TAG_GET_POWER_STATE,
    .data_sz = sizeof(msg.data),
    .data[0] = id,
    .end     = TAG_PROPERTY_END,
  };

  result r = do_ioctl(&msg);
  if (r != SUCCESS) {
    return FAILURE;
  }

  LOG("%s: %s, %s",
      name,
      (msg.data[1] & 0x1) ? "On" : "Off",
      (msg.data[1] & 0x2) ? "Absent" : "Present");

  return SUCCESS;
}

static result
info_clocks(u32 id, const char *name) {
  struct {
    u32 msg_sz;
    u32 status;
    u32 state_tag;
    u32 state_data_sz;
    u32 state_resp_sz;
    u32 state_data[2];
    u32 rate_tag;
    u32 rate_data_sz;
    u32 rate_resp_sz;
    u32 rate_data[2];
    u32 max_tag;
    u32 max_data_sz;
    u32 max_resp_sz;
    u32 max_data[2];
    u32 min_tag;
    u32 min_data_sz;
    u32 min_resp_sz;
    u32 min_data[2];
    u32 turbo_tag;
    u32 turbo_data_sz;
    u32 turbo_resp_sz;
    u32 turbo_data[2];
    u32 end;
  } msg = {
    .msg_sz        = sizeof(msg),
    .status        = STATUS_REQUEST,
    .state_tag     = TAG_GET_CLOCK_STATE,
    .state_data_sz = sizeof(msg.state_data),
    .state_data[0] = id,
    .rate_tag      = TAG_GET_CLOCK_RATE,
    .rate_data_sz  = sizeof(msg.rate_data),
    .rate_data[0]  = id,
    .max_tag       = TAG_GET_CLOCK_MAX,
    .max_data_sz   = sizeof(msg.max_data),
    .max_data[0]   = id,
    .min_tag       = TAG_GET_CLOCK_MIN,
    .min_data_sz   = sizeof(msg.min_data),
    .min_data[0]   = id,
    .turbo_tag     = TAG_GET_CLOCK_TURBO,
    .turbo_data_sz = sizeof(msg.turbo_data),
    .turbo_data[0] = id,
    .end           = TAG_PROPERTY_END,
  };

  result r = do_ioctl(&msg);
  if (r != SUCCESS) {
    return FAILURE;
  }

  LOG("%s: %s, %s, Rate %.2f MHz, Max %.2f MHz, Min %.2f MHz, Turbo %s",
      name,
      (msg.state_data[1] & 0x1) ? "On" : "Off",
      (msg.state_data[1] & 0x2) ? "Absent" : "Present",
      msg.rate_data[1] / 1000.0 / 1000.0,
      msg.max_data[1] / 1000.0 / 1000.0,
      msg.min_data[1] / 1000.0 / 1000.0,
      (msg.turbo_data[1] & 0x1) ? "On" : "Off");

  return SUCCESS;
}

static result
qpu_enable(opt o, bool enable) {
  struct {
    u32 msg_sz;
    u32 status;
    u32 tag;
    u32 data_sz;
    u32 resp_sz;
    u32 data;
    u32 end;
  } msg = {
    .msg_sz  = sizeof(msg),
    .status  = STATUS_REQUEST,
    .tag     = TAG_QPU_ENABLE,
    .data_sz = sizeof(msg.data),
    .data    = enable ? 1 : 0,
    .end     = TAG_PROPERTY_END,
  };

  if (o.verbose) {
    int fd = o.executing ? STDERR_FILENO : STDOUT_FILENO;
    LOGTO(fd, "%s QPUs", enable ? "Enabling" : "Disabling");
  }

  result r = do_ioctl(&msg);
  if (r != SUCCESS) {
    return FAILURE;
  }

  if (msg.data == FW_TIMEOUT) {
    ERROR("Firmware: Timeout");
    return FAILURE;
  } else if (msg.data != FW_SUCCESS) {
    ERROR("Firmware: Unspecified error");
    return FAILURE;
  }

  return SUCCESS;
}

result
mbox_exec_qpu(u32 ntasks, uaddr control, bool noflush, u32 timeout_ms) {
  struct {
    u32 msg_sz;
    u32 status;
    u32 tag;
    u32 data_sz;
    u32 resp_sz;
    u32 data[4];
    u32 end;
  } msg = {
    .msg_sz  = sizeof(msg),
    .status  = STATUS_REQUEST,
    .tag     = TAG_EXEC_QPU,
    .data_sz = sizeof(msg.data),
    .data    = {ntasks, control, noflush ? 1 : 0, timeout_ms},
    .end     = TAG_PROPERTY_END,
  };

  result r = do_ioctl(&msg);
  if (r != SUCCESS) {
    return FAILURE;
  }

  if (msg.data[0] == FW_TIMEOUT) {
    ERROR("Firmware: Timeout");
    return FAILURE;
  } else if (msg.data[0] != FW_SUCCESS) {
    ERROR("Firmware: Unspecified error");
    return FAILURE;
  }

  return SUCCESS;
}

result
mbox_unlock(u32 handle) {
  struct {
    u32 msg_sz;
    u32 status;
    u32 tag;
    u32 data_sz;
    u32 resp_sz;
    u32 data;
    u32 end;
  } msg = {
    .msg_sz  = sizeof(msg),
    .status  = STATUS_REQUEST,
    .tag     = TAG_MEM_UNLOCK,
    .data_sz = sizeof(msg.data),
    .data    = handle,
    .end     = TAG_PROPERTY_END,
  };

  result r = do_ioctl(&msg);
  if (r != SUCCESS) {
    return FAILURE;
  }

  if (msg.data == FW_TIMEOUT) {
    ERROR("Firmware: Timeout");
    return FAILURE;
  } else if (msg.data != FW_SUCCESS) {
    ERROR("Firmware: Unspecified error");
    return FAILURE;
  }

  return SUCCESS;
}

result
mbox_lock(uaddr *bus, u32 handle) {
  struct {
    u32 msg_sz;
    u32 status;
    u32 tag;
    u32 data_sz;
    u32 resp_sz;
    u32 data;
    u32 end;
  } msg = {
    .msg_sz  = sizeof(msg),
    .status  = STATUS_REQUEST,
    .tag     = TAG_MEM_LOCK,
    .data_sz = sizeof(msg.data),
    .data    = handle,
    .end     = TAG_PROPERTY_END,
  };

  result r = do_ioctl(&msg);
  if (r != SUCCESS) {
    return FAILURE;
  }

  *bus = (uaddr)msg.data;

  return SUCCESS;
}

result
mbox_free(u32 handle) {
  struct {
    u32 msg_sz;
    u32 status;
    u32 tag;
    u32 data_sz;
    u32 resp_sz;
    u32 data;
    u32 end;
  } msg = {
    .msg_sz  = sizeof(msg),
    .status  = STATUS_REQUEST,
    .tag     = TAG_MEM_FREE,
    .data_sz = sizeof(msg.data),
    .data    = handle,
    .end     = TAG_PROPERTY_END,
  };

  result r = do_ioctl(&msg);
  if (r != SUCCESS) {
    return FAILURE;
  }

  if (msg.data == FW_TIMEOUT) {
    ERROR("Firmware: Timeout");
    return FAILURE;
  } else if (msg.data != FW_SUCCESS) {
    ERROR("Firmware: Unspecified error");
    return FAILURE;
  }

  return SUCCESS;
}

result
mbox_alloc(u32 *handle, u32 size, u32 align) {
  struct {
    u32 msg_sz;
    u32 status;
    u32 tag;
    u32 data_sz;
    u32 resp_sz;
    u32 data[3];
    u32 end;
  } msg = {
    .msg_sz  = sizeof(msg),
    .status  = STATUS_REQUEST,
    .tag     = TAG_MEM_ALLOC,
    .data_sz = sizeof(msg.data),
    .data    = {size, align, MEM_L2ALLOC},
    .end     = TAG_PROPERTY_END,
  };

  result r = do_ioctl(&msg);
  if (r != SUCCESS) {
    return FAILURE;
  }

  *handle = msg.data[0];

  return SUCCESS;
}

result
mbox_voltage(opt o) {
  result r;

  if (o.verbose)
    DIVIDER("Voltage");

  r = info_voltage(VOLT_CORE, "Core");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_voltage(VOLT_SDRAM_CORE, "SDRAM-Core");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_voltage(VOLT_SDRAM_PHY, "SDRAM-Phy");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_voltage(VOLT_SDRAM_IO, "SDRAM-I/O");
  if (r != SUCCESS) {
    return FAILURE;
  }

  return SUCCESS;
}

result
mbox_version(opt o) {
  struct {
    u32 msg_sz;
    u32 status;
    u32 rev_tag;
    u32 rev_data_sz;
    u32 rev_resp_sz;
    u32 rev_data;
    u32 var_tag;
    u32 var_data_sz;
    u32 var_resp_sz;
    u32 var_data;
    u32 end;
  } msg = {
    .msg_sz      = sizeof(msg),
    .status      = STATUS_REQUEST,
    .rev_tag     = TAG_GET_FW_REVISION,
    .rev_data_sz = sizeof(msg.rev_data),
    .var_tag     = TAG_GET_FW_VARIANT,
    .var_data_sz = sizeof(msg.var_data),
    .end         = TAG_PROPERTY_END,
  };

  result r = do_ioctl(&msg);
  if (r != SUCCESS) {
    return FAILURE;
  }

  if (o.verbose)
    DIVIDER("Firmware");

  LOG("Revision: %#x", msg.rev_data);
  LOG("Variant: %#x", msg.var_data);

  return SUCCESS;
}

result
mbox_temp(opt o) {
  struct {
    u32 msg_sz;
    u32 status;
    u32 temp_tag;
    u32 temp_data_sz;
    u32 temp_resp_sz;
    u32 temp_data[2];
    u32 max_tag;
    u32 max_data_sz;
    u32 max_resp_sz;
    u32 max_data[2];
    u32 end;
  } msg = {
    .msg_sz       = sizeof(msg),
    .status       = STATUS_REQUEST,
    .temp_tag     = TAG_GET_TEMP,
    .temp_data_sz = sizeof(msg.temp_data),
    .temp_data[0] = 0,  // Temp ID
    .max_tag      = TAG_GET_TEMPMAX,
    .max_data_sz  = sizeof(msg.max_data),
    .max_data[0]  = 0,  // Temp ID
    .end          = TAG_PROPERTY_END,
  };

  result r = do_ioctl(&msg);
  if (r != SUCCESS) {
    return FAILURE;
  }

  if (o.verbose)
    DIVIDER("Temperature");

  LOG("%.1f C (Max %.1f C)",
      msg.temp_data[1] / 1000.0,
      msg.max_data[1] / 1000.0);

  return SUCCESS;
}

result
mbox_power(opt o) {
  result r;

  if (o.verbose)
    DIVIDER("Power");

  r = info_power(POWER_SD_CARD, "SDCARD");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_power(POWER_UART0, "UART0");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_power(POWER_UART1, "UART1");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_power(POWER_USB_HCD, "USBHCD");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_power(POWER_I2C0, "I2C0");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_power(POWER_I2C1, "I2C1");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_power(POWER_I2C2, "I2C2");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_power(POWER_SPI, "SPI");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_power(POWER_CCP2TX, "CCP2TX");
  if (r != SUCCESS) {
    return FAILURE;
  }

  return SUCCESS;
}

result
mbox_memory(opt o) {
  struct {
    u32 msg_sz;
    u32 status;
    u32 arm_tag;
    u32 arm_data_sz;
    u32 arm_resp_sz;
    u32 arm_data[2];
    u32 vc4_tag;
    u32 vc4_data_sz;
    u32 vc4_resp_sz;
    u32 vc4_data[2];
    u32 end;
  } msg = {
    .msg_sz      = sizeof(msg),
    .status      = STATUS_REQUEST,
    .arm_tag     = TAG_GET_MEM_ARM,
    .arm_data_sz = sizeof(msg.arm_data),
    .vc4_tag     = TAG_GET_MEM_VC4,
    .vc4_data_sz = sizeof(msg.vc4_data),
    .end         = TAG_PROPERTY_END,
  };

  result r = do_ioctl(&msg);
  if (r != SUCCESS) {
    return FAILURE;
  }

  if (o.verbose)
    DIVIDER("Memory");

  LOG("ARM: %u MiB, Base Address: %#x",
      msg.arm_data[1] / 1024 / 1024,
      msg.arm_data[0]);
  LOG("GPU: %u MiB, Base Address: %#x",
      msg.vc4_data[1] / 1024 / 1024,
      msg.vc4_data[0]);

  return SUCCESS;
}

result
mbox_clocks(opt o) {
  result r;

  if (o.verbose)
    DIVIDER("Clocks");

  r = info_clocks(CLOCK_EMMC, "EMMC");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_clocks(CLOCK_UART, "UART");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_clocks(CLOCK_ARM, "ARM");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_clocks(CLOCK_CORE, "CORE");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_clocks(CLOCK_V3D, "V3D");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_clocks(CLOCK_H264, "H264");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_clocks(CLOCK_ISP, "ISP");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_clocks(CLOCK_SDRAM, "SDRAM");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_clocks(CLOCK_PIXEL, "PIXEL");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_clocks(CLOCK_PWM, "PWM");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_clocks(CLOCK_HEVC, "HEVC");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_clocks(CLOCK_EMMC2, "EMMC2");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_clocks(CLOCK_M2MC, "M2MC");
  if (r != SUCCESS) {
    return FAILURE;
  }

  r = info_clocks(CLOCK_PIXEL_BVB, "PIXELBVB");
  if (r != SUCCESS) {
    return FAILURE;
  }

  return SUCCESS;
}

result
mbox_board(opt o) {
  struct {
    u32 msg_sz;
    u32 status;
    u32 model_tag;
    u32 model_data_sz;
    u32 model_resp_sz;
    union64 model_data;
    u32 rev_tag;
    u32 rev_data_sz;
    u32 rev_resp_sz;
    union64 rev_data;
    u32 mac_tag;
    u32 mac_data_sz;
    u32 mac_resp_sz;
    union64 mac_data;
    u32 serial_tag;
    u32 serial_data_sz;
    u32 serial_resp_sz;
    union64 serial_data;
    u32 end;
  } __attribute__((packed)) msg = {
    .msg_sz         = sizeof(msg),
    .status         = STATUS_REQUEST,
    .model_tag      = TAG_GET_BD_MODEL,
    .model_data_sz  = sizeof(msg.model_data),
    .rev_tag        = TAG_GET_BD_REVISION,
    .rev_data_sz    = sizeof(msg.rev_data),
    .mac_tag        = TAG_GET_BD_MAC,
    .mac_data_sz    = sizeof(msg.mac_data),
    .serial_tag     = TAG_GET_BD_SERIAL,
    .serial_data_sz = sizeof(msg.serial_data),
    .end            = TAG_PROPERTY_END,
  };

  result r = do_ioctl(&msg);
  if (r != SUCCESS) {
    return FAILURE;
  }

  if (o.verbose)
    DIVIDER("Board");

  LOG("Model: %#x", msg.model_data.w[0]);
  LOG("Revision: %#x", msg.rev_data.w[0]);
  LOG("Serial: %#llx", msg.serial_data.l);
  LOG("MAC: %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
      msg.mac_data.b[0],
      msg.mac_data.b[1],
      msg.mac_data.b[2],
      msg.mac_data.b[3],
      msg.mac_data.b[4],
      msg.mac_data.b[5]);

  return SUCCESS;
}

result
mbox_disable(opt o) {
  return qpu_enable(o, false);
}

result
mbox_enable(opt o) {
  return qpu_enable(o, true);
}

result
mbox_cleanup(void) {
  bool error = false;

  if (G.vcio_fd) {
    int ret = close(G.vcio_fd);
    if (ret == -1) {
      ERROR("%s", strerror(errno));
      error = true;
    }
    G.vcio_fd = 0;
  }

  return error ? FAILURE : SUCCESS;
}

result
mbox_init(void) {
  int fd = open("/dev/vcio", O_RDWR);
  if (fd == -1) {
    if (errno == EACCES) {
      NOTICE("Need root");
      return FAILURE;
    } else {
      ERROR("%s", strerror(errno));
      return FAILURE;
    }
  }

  G.vcio_fd = fd;

  return SUCCESS;
}
