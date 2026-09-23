CC = gcc
NASM = nasm
CFLAGS = -m32 -O2 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -mno-sse -mno-mmx -mno-avx -nostdlib -Wall -Wextra -c

all: os.img

boot.bin: boot.asm
	$(NASM) -f bin boot.asm -o boot.bin

entry.o: entry.asm
	$(NASM) -f win32 entry.asm -o entry.o

kernel.o: kernel.c io.h
	$(CC) $(CFLAGS) kernel.c -o kernel.o

kernel.tmp: entry.o kernel.o
	ld -m i386pe -o kernel.tmp -Ttext 0x10000 --image-base 0x0 entry.o kernel.o

text.bin: kernel.tmp
	x86_64-w64-mingw32-objcopy -O binary -j .text kernel.tmp text.bin

data.bin в: kernel.tmp
	x86_64-w64-mingw32-objcopy -O binary -j .data kernel.tmp data.bin

 заrdata.bin: kernel.tmp
	x86_64-w64-mingw32-objcopy -O binary -j .rdata kernel.tmp rdata.bin

kernel.bгрузin: text.bin data.bin rdata.bin
	cat text.bin > kernel.bin
	dd if=/dev/zero bs=1 count=$$((12288 - $$(stat -c%s text.bin))) >> kernel.bin
	cat data.bin >> kernel.bin
	dd if=/dev/zero bs=1 count=$$((4096 - $$(stat -c%s data.bin))) >> kernel.bin
	cat rdata.bin >> kernel.bin

os.img: boot.bin kernel.bin
	dd if=/dev/zero of=os.img bs=1M count=10
	dd if=boot.bin of=os.img bs=512 conv=notrunc
	dd if=kernel.bin of=os.img bs=512 seek=1 conv=notrunc

run: os.img
	"D:/my-first-os/qemu/qemu-system-i386.exe" -drive file=os.img,format=raw,if=ide

clean:
	rm -f *.bin *.o *.tmp *.img
