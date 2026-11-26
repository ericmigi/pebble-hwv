/*
 * HWV loaded by pblboot: keep pblboot's boot bits sane, and hand the board back to it for USB DFU.
 *
 * pblboot keeps its boot bits in the first word of retained RAM (0x20000000, 32 words, the last
 * one a CRC-32 of the others), which pblboot.overlay keeps out of HWV's RAM.
 */

#include <zephyr/init.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/reboot.h>

#define RETAINED       ((volatile uint32_t *)0x20000000UL)
#define RETAINED_CRC   31U
#define BIT_INITIALIZED BIT(0)
#define BITS_RESET_LOOP (BIT(11) | BIT(12) | BIT(13))
#define BIT_FW_STABLE  BIT(14)
#define BIT_FORCE_DFU  BIT(20)

static uint32_t retained_crc(void)
{
	return crc32_ieee((const uint8_t *)RETAINED, RETAINED_CRC * sizeof(uint32_t));
}

static void bootbits_update(uint32_t set, uint32_t clear)
{
	if (retained_crc() != RETAINED[RETAINED_CRC]) {
		/* Power was lost: pblboot starts the bits over */
		return;
	}
	RETAINED[0] = (RETAINED[0] & ~clear) | set | BIT_INITIALIZED;
	RETAINED[RETAINED_CRC] = retained_crc();
}

/* Like PebbleOS: started fine, so no reset loop and no start strikes */
static int pblboot_boot_ok(void)
{
	bootbits_update(BIT_FW_STABLE, BITS_RESET_LOOP);
	return 0;
}

SYS_INIT(pblboot_boot_ok, APPLICATION, 0);

static int cmd_dfu(const struct shell *sh, size_t argc, char **argv)
{
	if (retained_crc() != RETAINED[RETAINED_CRC]) {
		shell_error(sh, "pblboot boot bits not found");
		return -ENOENT;
	}
	bootbits_update(BIT_FORCE_DFU, 0U);
	shell_print(sh, "Restarting into pblboot USB DFU");
	k_msleep(100);
	sys_reboot(SYS_REBOOT_WARM);
	return 0;
}

SHELL_SUBCMD_ADD((hwv), dfu, NULL, "Restart into pblboot USB DFU", cmd_dfu, 1, 0);
