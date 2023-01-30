obj-m := test7.o
ccflags-y += -std=gnu99 -Wall -Wno-declaration-after-statement

_X_CC=~/BBB_Workspace/Downloads/buildroot-2022.11/output/host/opt/ext-toolchain/bin/arm-linux-gnueabihf-
KERNEL_SRC_DIR=~/BBB_Workspace/Downloads/buildroot-2022.11/output/build/linux-5.10.145-cip17-rt7/

all:TARGET1
TARGET1:test7.c
	@echo "**** Ichiri's Makefile ****"
	make ARCH=arm CROSS_COMPILE=$(_X_CC) -C $(KERNEL_SRC_DIR) M=$(shell pwd) modules
clean:
	@echo "*****Ichiri's Makefile clean ****"
	make ARCH=arm CROSS_COMPILE=$(_X_CC) -C $(KERNEL_SRC_DIR) M=$(shell pwd) clean

# ********THIS IS FOR WHEN COMPILING ON UBUNTU.  Not cross compile.
# make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules
