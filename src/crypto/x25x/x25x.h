#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

    // void x25x_hash(unsigned char* output, const unsigned char* input);
    void x25x_hash(unsigned char* output, const unsigned char* input, uint32_t len);
    void x22i_hash(uint8_t* output, const uint8_t* input);

#ifdef __cplusplus
}
#endif
