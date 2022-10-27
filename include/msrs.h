#ifndef __MSRS_H__
#define __MSRS_H__

#define MSR_STAR   (0xC0000081)
#define MSR_LSTAR  (0xC0000082)
#define MSR_CSTAR  (0xC0000083)
#define MSR_SFMASK (0xC0000084)

inline void set_msr(uint32_t msr, uint32_t h, uint32_t l)
{
    asm("wrmsr\n" :: "c"(msr), "d"(h), "a"(l));
}

inline uint64_t get_msr(uint32_t msr)
{
    uint32_t h, l;
    asm("rdmsr\n" :"=d"(h), "=a"(l):"c"(msr));

    return (h << 32) | l;
}
    
#endif
