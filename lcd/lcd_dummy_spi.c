/* drivers/video/sc8810/lcd_ili9486_rgb_spi.c
 * Copyright (C) 2010 Spreadtrum
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include "../sprdfb_panel.h"
#include "../sprdfb.h"
#include "../sprdfb_spi_intf.h"


#define DUMMY_SPI_SPEED        (48*1000*1000UL)


static int32_t dummy_init(struct panel_spec * self){
}
static int32_t dummy_enter_sleep(struct panel_spec *self, uint8_t is_sleep){
}
static int32_t dummy_set_window(struct panel_spec *self,
		uint16_t left, uint16_t top, uint16_t right, uint16_t bottom){
}
static int32_t dummy_invalidate(struct panel_spec *self){
	printk("dummy_invalidate\n");
}
static int32_t dummy_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom){
}

static int32_t dummy_read_id(struct panel_spec *self){
	return 0xffffffff;
}
static int32_t dummy_reset(struct panel_spec *self){
	return 0;
}

static struct panel_operations lcd_dummy_spi_operations = {
	.panel_init = dummy_init,
	.panel_set_window = dummy_set_window,
	.panel_invalidate_rect= dummy_invalidate_rect,
	.panel_invalidate = dummy_invalidate,
	.panel_enter_sleep = dummy_enter_sleep,
	.panel_readid          = dummy_read_id,
	.panel_reset		=dummy_reset,
};

static struct info_spi lcd_dummy_spi_info = {
	.bus_num = 0,
	.cs = 0,
	.cd_gpio = CONFIG_FB_SPI_CD_GPIO_NUM,
	.spi_mode = 1,
	.spi_pol_mode = SPI_MODE_0,
	.speed = DUMMY_SPI_SPEED,
	.en_softbigendian = 0,
};
static struct panel_spec lcd_dummy_spi_spec = {
	.width = 240,
	.height = 320,
	.bpp = 2,
	.fps = 35,
	.type = SPRDFB_PANEL_TYPE_SPI,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.spi = &lcd_dummy_spi_info
	},
	.ops = &lcd_dummy_spi_operations,
	.suspend_mode = SEND_SLEEP_CMD,
};
static struct panel_cfg lcd_dummy_spi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0xffffffff,
	.lcd_name = "lcd_dummy_spi",
	.panel = &lcd_dummy_spi_spec,
};
static int __init lcd_dummy_spi_init(void)
{
	return sprdfb_panel_register(&lcd_dummy_spi);
}

subsys_initcall(lcd_dummy_spi_init);

