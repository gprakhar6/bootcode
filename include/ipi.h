#ifndef __IPI_H__
#define __IPI_H__

int send_ipi_to_all(uint8_t vector);
int send_ipi_to(uint8_t id, uint8_t vector);

#endif
