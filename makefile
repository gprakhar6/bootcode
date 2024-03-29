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

#C_FLAGS := -Wall -Werror -ffreestanding -fno-builtin
C_FLAGS := -Wall -ffreestanding -fno-builtin

.PHONY : all main
all: depend dirtree main
depend: $(DEPENDS)

# -MF  write the generated dependency rule to a file
# -MG  assume missing headers will be generated and don't stop with an error
# -MM  generate dependency rule for prerequisite, skipping system headers
# -MP  add phony target for each header to prevent errors when header is missing
# -MT  add a target to the generated dependency
$(OBJDIR)/%.d: %.c
	gcc $(C_FLAGS) -Werror -O2 $(INCLUDE_FLAGS) -MM $< -MT $(OBJDIR)/$(<:.c=.o) -MF $@
include $(DEPENDS)

dirtree:
	@mkdir -p $(OBJDIR)
	@mkdir -p $(BINDIR)
	@for file in $(OBJS); do echo `dirname $$file`; done | sort -u | xargs -I {} mkdir -p {}

main: $(OBJS)
	ld -nostdlib -Tlinker.script -Map=$(BINDIR)/main.map $(OBJS) -o $(BINDIR)/main
	@objdump -D $(BINDIR)/main > $(BINDIR)/main.dump
	@cat $(BINDIR)/main.map | grep 'KERN_END = .' | awk '//{print $$0; printf("KERN_END = %d KB\n", $$1/1024);}'
	./func_mm_gen bin/main

$(OBJDIR)/%.o: %.S
	gcc $(INCLUDE_FLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.c
	gcc $(C_FLAGS) -O2 $(INCLUDE_FLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS) $(BINDIR)/main $(DEPENDS)
	find . -type f -name "*~" -or -name "#*"| xargs -I {} rm -rf {}

#	objcopy -O binary --only-section=.text main main.bin
