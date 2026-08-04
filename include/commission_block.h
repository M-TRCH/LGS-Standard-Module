#ifndef COMMISSION_BLOCK_H
#define COMMISSION_BLOCK_H

#include <stdint.h>

/*  @file include/commission_block.h
 *  @brief Values a host tool patches into the .bin before flashing over SWD.
 *
 *  A blank board has no firmware, so it cannot be told its Modbus ID over
 *  Modbus — that would need a second tool and a second session after the
 *  ST-Link flash. Settings live on the external AT24, which ST-Link cannot
 *  reach either. What ST-Link *can* write is the image itself, so the image
 *  carries the ID the board should adopt on first boot.
 *
 *  The block is found by its magic, never by guessing at field values: a host
 *  that scanned for `identifier == 247` would also have to know the
 *  surrounding defaults, and would silently start patching the wrong address
 *  the day one of them changed.
 *
 *  As built, token is 0: an un-patched image behaves exactly as it did before
 *  this block existed, and an OTA image can therefore never disturb an ID.
 *
 *  Two guarantees this file cannot make on its own:
 *    - tools/post_build_check.py fails the build unless exactly one intact
 *      block is present, so a toolchain that ever optimises it away breaks
 *      the build instead of shipping boards that cannot be commissioned.
 *    - src/svc/commission.cpp reads it byte-wise through a volatile pointer.
 *      If a future toolchain defeats that, move the definition to hand-written
 *      assembly (.section .rodata.lgs_commission) — GCC's LTO cannot see into
 *      assembly, so folding becomes impossible by construction.
 */

#define COMMISSION_MAGIC        "LGS-COMMISSION"    /* 14 chars, stored in char[16] */
#define COMMISSION_MAGIC_LEN    16
#define COMMISSION_VERSION      1

/* applyMask — which values the firmware should adopt. */
#define COMMISSION_APPLY_ID     0x0001u

/* flags */
#define COMMISSION_FLAG_FORCE   0x0001u  /* also renumber a board that already
                                          * has an ID; without this the block
                                          * only ever fills in an unset one */

/*  Every field after the magic is uint16_t, matching Settings — the host
 *  patches raw bytes, so there must be no padding for it to guess at. The
 *  token is 32-bit, carried as two halves to keep that property.
 */
typedef struct
{
    char     magic[COMMISSION_MAGIC_LEN];
    uint16_t version;       /* COMMISSION_VERSION */
    uint16_t size;          /* sizeof(CommissionBlock) */
    uint16_t tokenLo;       /* apply-once marker, low half  */
    uint16_t tokenHi;       /* high half; 0 = image not patched */
    uint16_t applyMask;     /* COMMISSION_APPLY_* */
    uint16_t flags;         /* COMMISSION_FLAG_* */
    uint16_t identifier;    /* Modbus slave ID to adopt */
    uint16_t crc;           /* CRC16-CCITT over every byte above */
} CommissionBlock;

#define COMMISSION_BLOCK_SIZE   32
#define COMMISSION_CRC_LEN      (COMMISSION_BLOCK_SIZE - 2)  /* all but crc */

/*  0 and 0xFFFFFFFF are reserved: an un-patched image reads 0, and erased
 *  flash or an erased EEPROM reads all-ones. Neither may mean "commission me".
 */
#define COMMISSION_TOKEN_NONE   0x00000000u
#define COMMISSION_TOKEN_ERASED 0xFFFFFFFFu

#endif /* COMMISSION_BLOCK_H */
