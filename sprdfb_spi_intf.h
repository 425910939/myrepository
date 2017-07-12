/* MicroArray Fingerprint
 * plat-mtk.h
 * date: 2015-08-20
 * version: v2.0
 * Author: czl
 */

#ifndef PLAT_GENERAL_H
#define PLAT_GENERAL_H

#include <linux/spi/spi.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <asm/unaligned.h>
#include <asm/io.h>
#include <mach/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <soc/sprd/regulator.h>
#include <linux/input/ist30xxb.h>
#include <soc/sprd/sci_glb_regs.h>
#include "sprdfb_panel.h"
#include <linux/completion.h>

#define MODE_NAME  "SPIDEV"

extern struct sprd_spi_intf *spi_intf;
extern int sci_glb_set(unsigned long reg, u32 bit);
extern int sci_glb_clr(unsigned long reg, u32 bit);

int sprd_register_spi_intf_client(struct panel_spec *panel);

struct sprd_spi_intf{
	struct spi_device *spi_dev;
	struct panel_spec* panel;
	struct completion spi_complete;
	int (*intf_config)(struct panel_spec *panel);
	void *current_base;
	uint32_t refresh_len;
	uint32_t lcm_id;
};

#endif



