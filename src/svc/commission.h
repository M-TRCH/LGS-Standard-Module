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

/*  @brief The device type this board was commissioned as, or 0 if it never
 *         was — callers fall back to the compile-time DEVICE_TYPE then.
 *
 *  Lives in the AT24 provisioning record, not in Settings: it describes what
 *  the factory fitted (LED mask vs ring + OLED + big button), so it is set
 *  once at commissioning and is not a value the bus should be able to change.
 */
uint16_t commissionDeviceType();

/*  @brief Tell the type resolver what this board physically has.
 *
 *  Call once at boot, immediately after the OLED probe and before anything
 *  reads deviceTypeEffective(). One firmware image serves both cabinets and
 *  works out which it is running on from the hardware rather than from a
 *  build flag or a commissioning step that can be skipped or get it wrong.
 */
void deviceTypeNoteDisplay(bool fitted);

/*  @brief The type this board actually is: what the boot probe found fitted,
 *         falling back to the commissioned type and then to the compile-time
 *         DEVICE_TYPE.
 *
 *  This is what reg 0 reports and what decides which display the board
 *  drives — a STANDARD cabinet is built with the 8-LED index mask and no
 *  ring, OLED or big button, so it must not also light a ring that is not
 *  there (and, on a bench board that has one, must not mirror onto it).
 *
 *  No OLED on I2C2 therefore means STANDARD, whatever the record says. The
 *  cost of that rule is that a ring board whose OLED has failed will also
 *  stop lighting its ring instead of just losing its display; the two parts
 *  are fitted together, so that is treated as one broken board, not two.
 */
uint16_t deviceTypeEffective();

/*  @brief Adopt the ID a host tool patched into the image, if it applies.
 *
 *  Call once at boot, right after settingsInit() and before anything reads
 *  settings().identifier. Does nothing at all unless the image was patched,
 *  the AT24 is healthy, and the board has no ID of its own yet — so a normal
 *  build, an OTA image, and an already-numbered board are all untouched.
 */
void commissionApplyAtBoot();

#endif // SVC_COMMISSION_H
