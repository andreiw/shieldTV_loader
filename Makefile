CROSS_COMPILE ?= ~/src/rpi3/gcc-linaro-5.5.0-2017.10-i686_aarch64-linux-gnu/bin/aarch64-linux-gnu-

CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

TARGET = shieldTV_loader
TEXT_BASE = 0x0

CFLAGS = \
	-march=armv8-a \
	-mlittle-endian \
	-fno-stack-protector \
	-mgeneral-regs-only \
	-mstrict-align \
	-fno-common \
	-fno-builtin \
	-ffreestanding \
	-std=gnu99 \
	-Werror \
	-Wall \
	-I ./

LDFLAGS = -pie

%.o: %.S
	$(CC) $(CFLAGS) $< -c -o $@

%.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@

all: $(TARGET)

$(TARGET): $(TARGET).bin
	mkbootimg --pagesize 4096 --kernel $< --ramdisk /dev/null -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -v -O binary $< $@

$(TARGET).elf: start.o main.o string.o fdt.o ctype.o fdt_ro.o fdt_strerror.o vsprintf.o cfb_console.o usbd.o lib.o fb.o lmb.o
	$(LD) -T boot.lds -Ttext=$(TEXT_BASE) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o $(TARGET) $(TARGET).* *~

.PHONY: clean
