export ARCH?=riscv64

ifeq ($(ARCH),riscv64)
export CROSS_COMPILE?=riscv64-linux-gnu-
else ifeq ($(ARCH),arm64)
export CROSS_COMPILE?=aarch64-linux-gnu-
else ifeq ($(ARCH),arm)
export CROSS_COMPILE?=arm-linux-gnueabihf-
else ifeq ($(ARCH),x86)
export CROSS_COMPILE?=
else
	$(error "Unsupported ARCH: $(ARCH)")
endif


export TARGET_SYSROOT?=output/sysroot