#ifndef SVC_COMMISSION_H
#define SVC_COMMISSION_H

#include "commission_block.h"

/*  @file svc/commission.h
 *  @brief Reading the commissioning block a host tool patched into the image.
 *
 *  See include/commission_block.h for what the block is and why it exists.
 */

/*  @brief Copy the block out of flash and validate magic, version, size and
 *         CRC.
 *  @param out Receives the block when this returns true; untouched otherwise.
 *  @return true when the image carries an intact block. A block with token 0
 *          is still intact — it just means nothing patched it, which callers
 *          check separately.
 */
bool commissionRead(CommissionBlock *out);

/*  @brief The block's 32-bit apply-once token, reassembled from its halves. */
uint32_t commissionToken(const CommissionBlock *b);

/*  @brief Adopt the ID a host tool patched into the image, if it applies.
 *
 *  Call once at boot, right after settingsInit() and before anything reads
 *  settings().identifier. Does nothing at all unless the image was patched,
 *  the AT24 is healthy, and the board has no ID of its own yet — so a normal
 *  build, an OTA image, and an already-numbered board are all untouched.
 */
void commissionApplyAtBoot();

#endif // SVC_COMMISSION_H
