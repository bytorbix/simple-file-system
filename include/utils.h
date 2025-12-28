#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdbool.h>


static inline void flip_bit(uint32_t *bitmap, int offset) 
{
    if (offset < 0 || offset > 31) {
        // Handle invalid offset, perhaps assert or return early.
        return; 
    }

    // Create the Mask: Shift 1 to the correct position (e.g., 1 << 5)
    uint32_t mask = 1U << offset;

    // Use Bitwise XOR to flip the bit
    // X ^ 1 = ~X (flips the bit)
    // X ^ 0 = X  (leaves the bit unchanged)
    *bitmap ^= mask;
}

static inline void set_bit(uint32_t *bitmap, int block, int bit_to_set) 
{
    int word_index = block / BITS_PER_WORD;
    int offset = block % BITS_PER_WORD;

    if (bit_to_set == 1) {
        bitmap[word_index] |= (1U << offset);
    }
    else if (bit_to_set == 0) {
        if (offset >= 0 && offset <= 31) {
            // 1. Create the Mask: 1 shifted to the target position (e.g., 0x00000020 for offset 5)
            uint32_t mask = 1U << offset;

            // 2. Invert the Mask: ~mask sets the target bit to 0 and all other 31 bits to 1.
            //    (e.g., ~0x00000020 = 0xFFFFFFDF)
            
            // 3. Bitwise AND: ANDing the word with this inverted mask
            //    (X & 0) always results in 0 (clears the target bit).
            //    (X & 1) leaves all other bits unchanged.
            bitmap[word_index] &= ~mask; // Correctly clears the bit to 0
        }
    }
}

static inline bool get_bit(uint32_t *bitmap, int block) 
{
    int word_index = block / BITS_PER_WORD;
    int offset = block % BITS_PER_WORD;

    if (offset < 0 || offset >= BITS_PER_WORD) {
        return false;
    }

    uint32_t mask = 1U << offset;

    return (bitmap[word_index] & mask) != 0;
}




#endif