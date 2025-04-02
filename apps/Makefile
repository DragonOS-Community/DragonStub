#
#  Copyright (C) 1999-2001 Hewlett-Packard Co.
#	Contributed by David Mosberger <davidm@hpl.hp.com>
#	Contributed by Stephane Eranian <eranian@hpl.hp.com>
#
#    All rights reserved.
#
#    Redistribution and use in source and binary forms, with or without
#    modification, are permitted provided that the following conditions
#    are met:
#
#    * Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
#      copyright notice, this list of conditions and the following
#      disclaimer in the documentation and/or other materials
#      provided with the distribution.
#    * Neither the name of Hewlett-Packard Co. nor the names of its
#      contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#
#    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
#    CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
#    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
#    MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
#    BE LIABLE FOR ANYDIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
#    OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
#    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
#    THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
#    SUCH DAMAGE.
#

SRCDIR = .

VPATH = $(SRCDIR)
TOPDIR = $(SRCDIR)/..

include $(SRCDIR)/../Make.defaults

LINUX_HEADERS	= /usr/src/sys/build
APPSDIR		= $(LIBDIR)/gnuefi/apps
CPPFLAGS	+= -D__KERNEL__ -I$(LINUX_HEADERS)/include
CRTOBJS		= $(TOPDIR)/$(ARCH)/gnuefi/crt0-efi-$(ARCH).o

LDSCRIPT	= $(TOPDIR)/gnuefi/elf_$(ARCH)_efi.lds
ifneq (,$(findstring FreeBSD,$(OS)))
LDSCRIPT	= $(TOPDIR)/gnuefi/elf_$(ARCH)_fbsd_efi.lds
endif

LDFLAGS		+= -shared -Bsymbolic -L$(TOPDIR)/$(ARCH)/lib -L$(TOPDIR)/$(ARCH)/gnuefi $(CRTOBJS)

LOADLIBES	+= -lefi -lgnuefi
LOADLIBES	+= $(LIBGCC)
LOADLIBES	+= -T $(LDSCRIPT)

TARGET_BSDRIVERS = drv0.efi
TARGET_RTDRIVERS =

ifneq ($(HAVE_EFI_OBJCOPY),)

FORMAT		:= --target efi-app-$(ARCH)
$(TARGET_BSDRIVERS): FORMAT=--target efi-bsdrv-$(ARCH)
$(TARGET_RTDRIVERS): FORMAT=--target efi-rtdrv-$(ARCH)

else

SUBSYSTEM	:= 0xa
$(TARGET_BSDRIVERS): SUBSYSTEM = 0xb
$(TARGET_RTDRIVERS): SUBSYSTEM = 0xc

FORMAT		:= -O binary
LDFLAGS		+= --defsym=EFI_SUBSYSTEM=$(SUBSYSTEM)

endif



DRAGON_STUB_FILES:= dragon_stub-main.c stub.c helper.c fdt.c secureboot.c elf.c mem.c alignedmem.c random.c
DRAGON_STUB_FILES += lib/vsprintf.c lib/hexdump.c lib/ctype.c lib/cmdline.c lib/string.c
__LIBFDT_DIR=lib/libfdt
DRAGON_STUB_FILES += $(__LIBFDT_DIR)/fdt_addresses.c $(__LIBFDT_DIR)/fdt_empty_tree.c $(__LIBFDT_DIR)/fdt_overlay.c $(__LIBFDT_DIR)/fdt_ro.c \
						$(__LIBFDT_DIR)/fdt_rw.c $(__LIBFDT_DIR)/fdt_strerror.c $(__LIBFDT_DIR)/fdt_sw.c $(__LIBFDT_DIR)/fdt_wip.c \
						$(__LIBFDT_DIR)/fdt.c

INCDIR += -I$(TOPDIR)/apps/lib/libfdt

ifeq ($(ARCH), riscv64)
	DRAGON_STUB_FILES += riscv-stub.c
	INCDIR += -I$(TOPDIR)/inc/dragonstub/linux/arch/riscv
endif

# 把*.c的列表转换为*.o的列表
DRAGON_STUB_OBJS := $(patsubst %.c,%.o,$(DRAGON_STUB_FILES))
PAYLOAD_ELF_OBJ=
# 将'/', '.', '-'替换为'_'
PAYLOAD_PATH_REPLACEMENT=_binary_$(shell echo "$(PAYLOAD_ELF)" | sed 's/\//_/g' | sed 's/\./_/g' | sed 's/\-/_/g')


dragon_stub: $(DRAGON_STUB_OBJS)
	@echo "Building dragon_stub..."

ifeq ($(PAYLOAD_ELF),)
	@echo "PAYLOAD_ELF is not set, not merging..."
	$(LD) $(LDFLAGS) $^ -o dragon_stub.so $(LOADLIBES)
else
# 把DragonStub和目标ELF合并
	@echo "Merging DragonStub and $(PAYLOAD_ELF)..."
	$(LD) -r -b binary $(PAYLOAD_ELF) -o payload.o.stage1 --no-relax
	$(OBJCOPY) --redefine-sym $(PAYLOAD_PATH_REPLACEMENT)_start=_binary_payload_start \
		   --redefine-sym $(PAYLOAD_PATH_REPLACEMENT)_end=_binary_payload_end \
		   --redefine-sym $(PAYLOAD_PATH_REPLACEMENT)_size=_binary_payload_size \
		   payload.o.stage1 payload.o
	$(LD) $(LDFLAGS) --no-relax $^ payload.o -o dragon_stub.so $(LOADLIBES)
	
endif
	
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .rodata -j .rel \
		    -j .rela -j .rel.* -j .rela.* -j .rel* -j .rela* \
		    -j .areloc -j .reloc $(FORMAT) dragon_stub.so dragon_stub.efi


TARGETS = $(TARGET_BSDRIVERS) $(TARGET_RTDRIVERS) dragon_stub 

all:	$(TARGETS)

ctors_test.so : ctors_fns.o ctors_test.o

clean:
	@rm -vf $(TARGETS) *~ *.o *.so

install:
	mkdir -p $(INSTALLROOT)$(APPSDIR)
	$(INSTALL) -m 644 $(TARGETS) $(INSTALLROOT)$(APPSDIR)

.PHONY: install

include $(SRCDIR)/../Make.rules
