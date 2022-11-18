#ifndef __APIC_H__
#define __APIC_H__

#define ICRL5(dm,tgm,mt,l,vec) ((tgm << 15) | (l << 14) | (dm << 11) | (mt << 8) | (vec))
#define ICRL3(mt,l,vec) ICRL5(0, 0, mt, l, vec)
#define ICRL2(l,vec) ICRL5(0, 0, 0, l, vec)
#define ICRL1(vec) ICRL5(0, 0, 0, 0, vec)
#define GET_ICRL_NAME(_1, _2, _3, _4, _5, NAME, ...) NAME
#define ICRL(...) GET_ICRL_NAME(__VA_ARGS__, ICRL5, ICRL4, ICRL3, ICRL2, ICRL1, ICRL0)(__VA_ARGS__)

inline uint8_t read_apic_bit_pack(apic_bit_pack_t *bit_pack, uint8_t vec) {
    uint8_t off, bitn;
    uint32_t ret;
    off = vec / 0x20;
    bitn = vec % 0x20;
    ret = (bit_pack[off].bits) & (uint32_t)(0x1 << bitn);
    return (ret==0)?(uint8_t)0:(uint8_t)1;
}

inline void set_apic_bit_pack(apic_bit_pack_t *bit_pack, uint8_t vec) {
    uint8_t off, bitn;
    off = vec / 0x20;
    bitn = vec % 0x20;
    bit_pack[off].bits |= (uint32_t)(0x1 << bitn);
}

inline void reset_apic_bit_pack(apic_bit_pack_t *bit_pack, uint8_t vec) {
    uint8_t off, bitn;
    off = vec / 0x20;
    bitn = vec % 0x20;
    bit_pack[off].bits ^= ~((uint32_t)(0x1 << bitn));
}


#endif
