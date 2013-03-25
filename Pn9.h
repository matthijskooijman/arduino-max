
/* How many PN9 bytes are included in the lookuptable. When changing
 * this, also add the extra bytes in Pn9.cpp. */
#define PN9_LEN 50

/**
 * Xor the first len bytes in buf with the PN9 sequence.
 *
 * Returns 0 if succesful or -1 if the buffer is longer than PN9_LEN.
 */
int xor_pn9(uint8_t *buf, size_t len);

/* vim: set sw=2 sts=2 expandtab: */
