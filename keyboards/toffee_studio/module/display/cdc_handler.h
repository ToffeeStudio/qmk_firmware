// display/cdc_handler.h
#ifndef CDC_HANDLER_H
#define CDC_HANDLER_H

#include <stdint.h>

void cdc_handler_init(void);
void virtser_recv(const uint8_t ch);

#endif // CDC_HANDLER_H
