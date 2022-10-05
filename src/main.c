#include <stdint.h>
#include <stddef.h>
#include "printf.h"
#include "util.h"
#include "hw_types.h"
#include "globvar.h"

int user_test_function();
void init_boot();

extern void *__gdt64_start;
struct tss_entry_t tss;
int main()
{
    init_boot();
    return 0;
}

void fill_tss()
{
    memset(&tss, 0, sizeof(tss));
    tss.rsp0 = stack_start - 16; // keep some 16 bytes free
    tss.iomap_base = 0;
    flush_tss(&tss);
}
void init_boot()
{
    fill_tss();
    //fill_user_mode_gdt();
}

int user_test_function()
{
    return 0;
}
