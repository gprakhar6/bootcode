#all:
#	gcc -c boot.S -o boot.o
#	gcc -Wall -Werror -ffreestanding -fno-builtin -c main.c -o main.o
#	ld -nostdlib -Tlinker.script main.o boot.o -o main
#	objdump -D main > main.dump
#	objcopy -O binary --only-section=.text main main.bin
#	gcc -Wall -Werror -nostdlib -ffreestanding -fno-builtin -Tlinker.script main.c -o main

OBJDIR := obj
BINDIR := bin
C_FILES := $(shell find * -type f -name "*.c")
ASM_FILES := $(shell find * -type f -name "*.S")
OBJS := $(patsubst %.c,%.o,$(addprefix $(OBJDIR)/, $(C_FILES))) \
	$(patsubst %.S,%.o,$(addprefix $(OBJDIR)/, $(ASM_FILES)))

DEPENDS := $(patsubst %.c,%.d,$(addprefix $(OBJDIR)/, $(C_FILES)))

INCLUDE_DIRS := $(shell find * -type f -name "*.h" -exec dirname {} \; | sort -u)
INCLUDE_FLAGS := $(addprefix -I, $(INCLUDE_DIRS))
.PHONY : all main
#all: depend dirtree main
all: depend
depend: $(DEPENDS)
$(OBJDIR)/%.d: %.c
	gcc -Wall -Werror -ffreestanding -fno-builtin -O2 $(INCLUDE_FLAGS) -MM $< -MF $@

dirtree:
	@mkdir -p $(OBJDIR)
	@mkdir -p $(BINDIR)
	@for file in $(OBJS); do echo `dirname $$file`; done | sort -u | xargs -I {} mkdir -p {}

main: $(OBJS)
	ld -nostdlib -Tlinker.script $(OBJS) -o $(BINDIR)/main
	objdump -D $(BINDIR)/main > $(BINDIR)/main.dump

$(OBJDIR)/%.o: %.S
	gcc $(INCLUDE_FLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.c
	gcc -Wall -Werror -ffreestanding -fno-builtin -O2 $(INCLUDE_FLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS) $(BINDIR)/main $(DEPENDS)
	find . -type f -name "*~" | xargs -I {} rm -rf {}

#	objcopy -O binary --only-section=.text main main.bin
