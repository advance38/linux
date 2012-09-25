# Default configuration (used if CONFIG not supplied)
# See common.h for docs
CONFIG ?= -DGRBTEST_KEY_TYPE=u32	\
	  -DGRBTEST_BUILD_GENERIC=0	\
	  -DGRBTEST_USE_LEFTMOST=1	\
	  -DGRBTEST_USE_RIGHTMOST=1	\
	  -DGRBTEST_USE_COUNT=1		\
	  -DGRBTEST_UNIQUE_KEYS=1	\
	  -DGRBTEST_INSERT_REPLACES=1	\
	  -DGRBTEST_USE_AUGMENTED=0

ifeq ($(KERNELRELEASE),)
    # Assume the source tree is where the running kernel was built
    # You should set KERNELDIR in the environment if it's elsewhere
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
endif

PWD := $(shell pwd)

# The below KERNEL_ARCH works on x86_64, but hasn't been tested elsewhere
# (e.g., x86 32-bit, ARM, etc.)
KERNEL_ARCH	 = $(shell uname -m | sed 's/x86_64/x86/')

# Kernel include directories
KERNEL_INCLUDES	 = -I$(KERNELDIR)/include \
		   -I$(KERNELDIR)/arch/$(KERNEL_ARCH)/include
CPPFLAGS	+= -DGRBTEST_USERLAND=1 -D__KERNEL__ -I$(PWD)/.. \
		   -I$(PWD)/overrides $(KERNEL_INCLUDES) $(CONFIG)
WARN_FLAGS	 = -Wall -Wundef -Wstrict-prototypes -Wno-unused-variable \
		   -Werror-implicit-function-declaration -Wno-trigraphs \
		   -Wno-format-security -Wno-unused-variable -Werror

# Standard CFLAGS if not already set
CFLAGS		?= -O2 -pipe
# FIXME: breaks glibc
#CFLAGS		+= -mno-see
# TODO: Can get tese from KBUILD_CFLAGS in arch/<arch>/Makefile?
#CFLAGS		+= -march=native -mno-mmx -mno-sse2 -mno-3dnow -mno-avx
CFLAGS		+= $(WARN_FLAGS) -fno-strict-aliasing -fno-common \
		   -fno-delete-null-pointer-checks
CC		?= gcc
CPPFLAGS	+= -DGRBTEST_CFLAGS="$(CFLAGS)" -DGRBTEST_CONFIG="$(CONFIG)" \
		   -DGRBTEST_CC="$(CC)" \
		   -DGRBTEST_ARCH="$(KERNEL_ARCH)" \
		   -DGRBTEST_ARCH_FLAGS="-march=k8 -m64" \
		   -DGRBTEST_PROCESSOR="$(shell uname -p)" \

all: grbtest

OBJ_FILES = main.o rbtree.o common.o facilities.o
HEADER_FILES = $(KERNELDIR)/include/linux/rbtree.h ../common.h

rbtree.c:
	ln -s $(KERNELDIR)/lib/rbtree.c $(PWD)/rbtree.c

common.c:
	ln -s ../common.c common.c

rbtree.o: rbtree.c $(KERNELDIR)/include/linux/rbtree.h

grbtest: $(OBJ_FILES) $(HEADER_FILES)
	$(CC) $(CFLAGS) $(OBJ_FILES) -o grbtest

clean:
	rm -f grbtest *.o rbtree.c common.c

