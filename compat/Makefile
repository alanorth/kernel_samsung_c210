obj-y += compat.o
#compat-objs :=

obj-n += compat_firmware_class.o

compat-y += main.o

# Compat kernel compatibility code
compat-n += compat-2.6.14.o
compat-n += compat-2.6.18.o
compat-n += compat-2.6.19.o
compat-n += compat-2.6.21.o
compat-n += compat-2.6.22.o
compat-n += compat-2.6.23.o
compat-n += compat-2.6.24.o
compat-n += \
	compat-2.6.25.o \
	pm_qos_params.o

compat-n += compat-2.6.26.o
compat-n += compat-2.6.27.o
compat-n += compat-2.6.28.o
compat-n += compat-2.6.29.o
compat-n += compat-2.6.32.o
compat-n += compat-2.6.33.o
compat-n += compat-2.6.35.o
compat-n += compat-2.6.36.o

compat-n += kfifo.o

compat-y += compat-2.6.37.o
compat-y += compat-2.6.38.o
compat-y += \
	compat-2.6.39.o \
	kstrtox.o
compat-y += compat-3.0.o

ifndef CONFIG_CORDIC
compat-y += cordic.o
endif
ifndef CONFIG_CRC8
compat-y += crc8.o
endif

ifndef CONFIG_64BIT
ifndef CONFIG_GENERIC_ATOMIC64
  compat-y += compat_atomic.o
endif
endif

EXTRA_CFLAGS += \
		-include $(KBUILD_SRC)/include/compat/compat-2.6.h \
        -DCOMPAT_BASE_TREE='"linux-stable.git"' \
        -DCOMPAT_BASE_TREE_VERSION='"v3.3"' \
        -DCOMPAT_PROJECT='"Compat-wireless"' \
        -DCOMPAT_VERSION='"compat-wireless-v3.3-2-n"'
