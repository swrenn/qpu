#
# Compile
#

TARGET := armhf
CFLAGS := -std=c11 -Wall -Werror -D_DEFAULT_SOURCE
LIBS   := -lbcm_host -lvchiq_arm -lvcos
BUILD  ?= release

ifeq ($(shell uname -m),armv6l)
	CC       := gcc
	INCDIRS  := -I /opt/vc/include
	LIBDIRS  := -L /opt/vc/lib
else
	CC       := toolchain/arm-rpi-4.9.3-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
	INCDIRS  := -I toolchain/libraspberrypi/opt/vc/include
	LIBDIRS  := -L toolchain/libraspberrypi/opt/vc/lib
endif

ifeq ($(BUILD),release)
	CFLAGS += -O2 -DNDEBUG -s
else ifeq ($(BUILD),debug)
	CFLAGS += -g
else ifeq ($(BUILD),noerror)
	CFLAGS += -g -Wno-error
else
	$(error Invalid value for BUILD)
endif

NAME      := qpu
SRCDIR    := src
SOURCES   := $(wildcard $(SRCDIR)/*.c)
BUILDROOT := $(TARGET)-$(BUILD)
BUILDDIR  := $(BUILDROOT)/build
BUILDSRC  := $(BUILDDIR)/$(SRCDIR)
OBJECTS   := $(SOURCES:%.c=$(BUILDDIR)/%.o)
DEPS      := $(SOURCES:%.c=$(BUILDDIR)/%.d)
BINARY    := $(BUILDDIR)/$(NAME)

.PHONY: bin
bin: $(BINARY)

$(BINARY): $(OBJECTS) | $(BUILDSRC)
	$(CC) $(CFLAGS) $(LIBDIRS) -o $@ $(OBJECTS) $(LIBS)

$(BUILDDIR)/%.o: %.c | $(BUILDSRC)
	$(CC) $(CFLAGS) $(INCDIRS) -MMD -c -o $@ $< $(LIBS)

-include $(DEPS)

$(BUILDSRC): | $(BUILDDIR)
	@install -d -m 0755 $(BUILDSRC)

$(BUILDDIR):
	@install -d -m 0755 $(BUILDDIR)

#
# Package
#

DESCRIPTION  := Raspberry Pi Video Core IV Command Line Utility
EXTDESC      := Explore the Raspberry Pi GPU from the command line.
MAINTAINER   := Samuel Wrenn <swrenn@gmail.com>
DEPENDS      := libc6 (>= 2.24), libraspberrypi0 (>= 1.20190517-1)
VERSION      := $(shell git describe)
ARCHITECTURE := $(TARGET)

PKGDIR       := $(BUILDROOT)/package
CTLDIR       := $(PKGDIR)/DEBIAN
BINDIR       := $(PKGDIR)/usr/bin
CMPLDIR      := $(PKGDIR)/usr/share/bash-completion/completions
CONTROL      := $(CTLDIR)/control

.PHONY: deb
deb: bin control | $(PKGDIR) $(CTLDIR) $(BINDIR) $(CMPLDIR) 
	@install -m 0755 $(BINARY) $(BINDIR)
	@install -m 0644 $(SRCDIR)/compl $(CMPLDIR)/qpu
	fakeroot dpkg-deb -Zgzip --build $(PKGDIR) $(BUILDROOT)

.PHONY: control
control: | $(CTLDIR)
	@truncate -s 0                           $(CONTROL)
	@echo "Package: $(NAME)"              >> $(CONTROL)
	@echo "Version: $(VERSION)"           >> $(CONTROL)
	@echo "Maintainer: $(MAINTAINER)"     >> $(CONTROL)
	@echo "Architecture: $(ARCHITECTURE)" >> $(CONTROL)
	@echo "Depends: $(DEPENDS)"           >> $(CONTROL)
	@echo "Description: $(DESCRIPTION)"   >> $(CONTROL)
	@echo " $(EXTDESC)"                   >> $(CONTROL)

$(BINDIR): | $(PKGDIR)
	@install -d -m 0755 $(BINDIR)

$(CTLDIR): | $(PKGDIR)
	@install -d -m 0755 $(CTLDIR)

$(CMPLDIR): | $(PKGDIR)
	@install -d -m 0755 $(CMPLDIR)

$(PKGDIR):
	@install -d -m 0755 $(PKGDIR)

#
# Other
#

.PHONY: default
default: bin

.PHONY: clean
clean:
	rm -rf armhf-release armhf-debug armhf-noerror
