#include <stdlib.h>
#include <assert.h>
#include "A52.h"

/* Masks for the four shift registers */
#ifndef R1MASK
    #define R1MASK	0x07FFFF /* 19 bits, numbered 0..18 */
#endif // R1MASK
#ifndef R2MASK
    #define R2MASK	0x3FFFFF /* 22 bits, numbered 0..21 */
#endif // R2MASK
#ifndef R3MASK
    #define R3MASK	0x7FFFFF /* 23 bits, numbered 0..22 */
#endif // R3MASK
#define R4MASK  0x01FFFF /* 17 bits, numbered 0..16 */

/* clocking bits of R4 */
#define R4TAP1  0x000400 /* bit 10 */
#define R4TAP2  0x000008 /* bit 3 */
#define R4TAP3  0x000080 /* bit 7 */

/* feedback taps for clocking shift registers */
#define R1TAPS  0x072000 /* bits 18,17,16,13 */
#define R2TAPS  0x300000 /* bits 21,20 */
#define R3TAPS  0x700080 /* bits 22,21,20,7 */
#define R4TAPS  0x010800 /* bits 16,11 */

/* The four shift registers.  They're in global variables to make the code
 * easier to understand.
 * A better implementation would not use global variables. */

word A52R1, A52R2, A52R3, A52R4;

/* Calculate the parity of a 32-bit word, i.e. the sum of its bits modulo 2
*/
bit parity(word x) {
    x ^= x>>16;
    x ^= x>>8;
    x ^= x>>4;
    x ^= x>>2;
    x ^= x>>1;
    return x&1;
}

/* Clock one shift register.  For A5/2, when the last bit of the frame
 * is loaded in, one particular bit of each register is forced to '1';
 * that bit is passed in as the last argument. */

word clockone(word reg, word mask, word taps, word loaded_bit) {
    word t = reg & taps;
    reg = (reg << 1) & mask;
    reg |= parity(t);
    reg |= loaded_bit;
    return reg;
}


/* Return 1 iff at least two of the parameter words are non-zero. */
bit majority(word w1, word w2, word w3) {
    int sum = (w1 != 0) + (w2 != 0) + (w3 != 0);
    if (sum >= 2)
        return 1;
    else
        return 0;
}



/* Clock two or three of R1,R2,R3, with clock control
 * according to their middle bits.
 * Specifically, we clock Ri whenever Ri's middle bit
 * agrees with the majority value of the three middle bits.  For A5/2,
 * use particular bits of R4 instead of the middle bits.  Also, for A5/2,
 * always clock R4.
 * If allP == 1, clock all three of R1,R2,R3, ignoring their middle bits.
 * This is only used for key setup.  If loaded == 1, then this is the last
 * bit of the frame number, and if we're doing A5/2, we have to set a
 * particular bit in each of the four registers. */
void clock(int allP, int loaded) {
    bit maj = majority(A52R4&R4TAP1, A52R4&R4TAP2, A52R4&R4TAP3);
    if (allP || (((A52R4&R4TAP1)!=0) == maj))
            A52R1 = clockone(A52R1, R1MASK, R1TAPS, loaded<<15);
    if (allP || (((A52R4&R4TAP2)!=0) == maj))
            A52R2 = clockone(A52R2, R2MASK, R2TAPS, loaded<<16);
    if (allP || (((A52R4&R4TAP3)!=0) == maj))
            A52R3 = clockone(A52R3, R3MASK, R3TAPS, loaded<<18);
    A52R4 = clockone(A52R4, R4MASK, R4TAPS, loaded<<10);
}




/* Generate an output bit from the current state.
 * You grab a bit from each register via the output generation taps;
 * then you XOR the resulting three bits.  For A5/2, in addition to
 * the top bit of each of R1,R2,R3, also XOR in a majority function
 * of three particular bits of the register (one of them complemented)
 * to make it non-linear.  Also, for A5/2, delay the output by one
 * clock cycle for some reason. */
bit A52getbit() {
    bit topbits = (((A52R1 >> 18) ^ (A52R2 >> 21) ^ (A52R3 >> 22)) & 0x01);
    static bit delaybit = 0;
    bit nowbit = delaybit;
    delaybit = (
        topbits
        ^ majority(A52R1&0x8000, (~A52R1)&0x4000, A52R1&0x1000)
        //^ majority((~A52R2)&0x10000, A52R2&0x2000, A52R2&0x200)
        ^ majority((~A52R2)&0x10000, A52R2&0x4000, A52R2&0x1000)
        ^ majority(A52R3&0x40000, A52R3&0x10000, (~A52R3)&0x2000)
        );
    return nowbit;
}


/* Do the A5 key setup.  This routine accepts a 64-bit key and
 * a 22-bit frame number. */
void A52keysetup(byte key[8], word frame) {
    int i;
    bit keybit, framebit;


    /* Zero out the shift registers. */
    A52R1 = A52R2 = A52R3 = 0;
    A52R4 = 0;


    /* Load the key into the shift registers,
     * LSB of first byte of key array first,
     * clocking each register once for every
     * key bit loaded.  (The usual clock
     * control rule is temporarily disabled.) */
    for (i=0; i<64; i++) {
        clock(1,0); /* always clock */
        keybit = (key[i/8] >> (i&7)) & 1; /* The i-th bit of the key */
        A52R1 ^= keybit; A52R2 ^= keybit; A52R3 ^= keybit;
        A52R4 ^= keybit;
    }


    /* Load the frame number into the shift registers, LSB first,
     * clocking each register once for every key bit loaded.
     * (The usual clock control rule is still disabled.)
     * For A5/2, signal when the last bit is being clocked in. */
    for (i=0; i<22; i++) {
        clock(1,i==21); /* always clock */
        framebit = (frame >> i) & 1; /* The i-th bit of the frame # */
        A52R1 ^= framebit; A52R2 ^= framebit; A52R3 ^= framebit;
        A52R4 ^= framebit;
    }


    /* Run the shift registers for 100 clocks
     * to mix the keying material and frame number
     * together with output generation disabled,
     * so that there is sufficient avalanche.
     * We re-enable the majority-based clock control
     * rule from now on. */
    for (i=0; i<100; i++) {
        clock(0,0);
    }
    /* For A5/2, we have to load the delayed output bit.  This does _not_
     * change the state of the registers.  For A5/1, this is a no-op. */
    A52getbit();


    /* Now the key is properly set up. */
}


/* Generate output.  We generate 228 bits of
 * keystream output.  The first 114 bits is for
 * the A->B frame; the next 114 bits is for the
 * B->A frame.  You allocate a 15-byte buffer
 * for each direction, and this function fills
 * it in. */
void A52run(byte AtoBkeystream[], byte BtoAkeystream[]) {
    int i;


    /* Zero out the output buffers. */
    for (i=0; i<=113/8; i++)
        AtoBkeystream[i] = BtoAkeystream[i] = 0;


    /* Generate 114 bits of keystream for the
     * A->B direction.  Store it, MSB first. */
    for (i=0; i<114; i++) {
        clock(0,0);
        AtoBkeystream[i/8] |= A52getbit() << (7-(i&7));
    }


    /* Generate 114 bits of keystream for the
     * B->A direction.  Store it, MSB first. */
    for (i=0; i<114; i++) {
        clock(0,0);
        BtoAkeystream[i/8] |= A52getbit() << (7-(i&7));
    }
}

void A52_GSM( byte *key, int klen, int count, byte *block1, byte *block2 )
{
	assert(klen == 64);
	A52keysetup(key, count); // TODO - frame and count are not the same
	A52run(block1, block2);
}

