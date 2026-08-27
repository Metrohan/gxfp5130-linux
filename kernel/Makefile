# SPDX-License-Identifier: GPL-2.0
ifeq ($(KBUILD_EXTMOD),)

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
MODULE_DIR := $(CURDIR)
ifeq ($(shell sed -n 's/^CONFIG_CC_IS_CLANG=y$$/y/p' $(KERNELDIR)/.config 2>/dev/null),y)
KBUILD_TOOLCHAIN := LLVM=1
endif

.PHONY: all modules clean

all modules:
	$(MAKE) $(KBUILD_TOOLCHAIN) -C $(KERNELDIR) M=$(MODULE_DIR) CONFIG_GXFP5130=m modules

clean:
	$(MAKE) $(KBUILD_TOOLCHAIN) -C $(KERNELDIR) M=$(MODULE_DIR) clean

endif

obj-$(CONFIG_GXFP5130) += gxfp.o

ccflags-y += -I$(src)/include

gxfp-y := \
	gxfp_main.o \
	transport/gxfp_espi_common.o \
	transport/gxfp_espi_tx.o \
	transport/gxfp_espi_rx_sync.o \
	transport/gxfp_espi_rx_irq.o \
	hw/gxfp_acpi.o \
	hw/gxfp_delay.o \
	hw/gxfp_mmio.o \
	hw/gxfp_gpio.o \
	proto/gxfp_mp_proto.o \
	proto/gxfp_goodix_proto.o \
	cmd/gxfp_cmd.o \
	cmd/gxfp_cmd_version.o \
	cmd/gxfp_cmd_mcu_state.o \
	cmd/gxfp_cmd_mcu_config.o \
	cmd/gxfp_cmd_reset.o \
	driver/gxfp_platform.o \
	driver/gxfp_irq.o \
	driver/gxfp_trace.o \
	driver/gxfp_uapi_fifo.o \
	driver/gxfp_uapi_ctrl.o \
	driver/gxfp_uapi_misc.o
