#ifndef QR_CODE_HPP
#define QR_CODE_HPP

#include <stdint.h>

bool generate_qr_code( int size, const char* text, uint32_t* out_buffer);

#endif
