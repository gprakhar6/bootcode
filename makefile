#all:
#	gcc -c boot.S -o boot.o
#	gcc -Wall -Werror -ffreestanding -fno-builtin -c main.c -o main.o
#	ld -nostdlib -Tlinker.script main.o boot.o -o main
#	objdump -D main > main.dump
#	objcopy -O binary --only-section=.text main main.bin
#	gcc -Wall -Werror -nostdlib -ffreestanding -fno-builtin -Tlinker.script main.c -o main

C_FILES := $(shell find . -type f -name "*.c")
OBJS := $(patsubst %.c,%.o,$(C_FILES)) boot.o

.PHONY : all main
all: main

main: $(OBJS)
	ld -nostdlib -Tlinker.script $(OBJS) -o main

boot.o: boot.S
	gcc -c boot.S -o boot.o

%.o: %.c
	gcc -Wall -Werror -ffreestanding -fno-builtin -c $< -o $@

clean:
	rm -rf *.o main

dump:
	objdump -D main > main.dump
#	objcopy -O binary --only-section=.text main main.bin
