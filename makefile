all:
	gcc -c boot.S -o boot.o
	gcc -Wall -Werror -ffreestanding -fno-builtin -c main.c -o main.o
	ld -nostdlib -Tlinker.script main.o boot.o -o main
	objdump -D main > main.dump
	objcopy -O binary --only-section=.text main main.bin
#	gcc -Wall -Werror -nostdlib -ffreestanding -fno-builtin -Tlinker.script main.c -o main

