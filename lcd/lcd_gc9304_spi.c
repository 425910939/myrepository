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

#define DATA_MAX  0xff
struct lcm_cmds{
uint8_t sleep;
uint8_t data[DATA_MAX]; //0,1 mean lens
};

#define GC9304_SPI_SPEED        (48*1000*1000UL)

#define LCM_GPIO_RSTN		(50)
static struct lcm_cmds init_code[] = {
	{0x00,0x00,0x01,0xfe},
	{0x00,0x00,0x01,0xef},
	{0x00,0x00,0x02,0x36,0x48},
	{0x00,0x00,0x02,0x3a,0x05},
	{0x00,0x00,0x03,0xe8,0x18,0x40},
	{0x00,0x00,0x02,0x35,0x00},
	{0x00,0x00,0x03,0xe3,0x01,0x04},
	{0x00,0x00,0x03,0xa5,0x40,0x40},
	{0x00,0x00,0x03,0xa4,0x44,0x44},
	{0x00,0x00,0x03,0xaa,0x88,0x88},
	{0x00,0x00,0x02,0xab,0x08},
	{0x00,0x00,0x02,0xae,0x0b},
	{0x00,0x00,0x02,0xac,0x00},
	{0x00,0x00,0x02,0xaf,0x77},
	{0x00,0x00,0x02,0xad,0x77},
	{0x00,0x00,0x07,0xf0,0x02,0x02,0x00,0x09,0x13,0x0e},
	{0x00,0x00,0x07,0xf1,0x01,0x02,0x00,0x0f,0x1c,0x10},
	{0x00,0x00,0x07,0xf2,0x0f,0x08,0x39,0x04,0x05,0x49},
	{0x00,0x00,0x07,0xf3,0x11,0x0A,0x41,0x03,0x03,0x4f},
	{0x00,0x00,0x07,0xf4,0x0F,0x18,0x16,0x1D,0x20,0x0f},
	{0x00,0x00,0x07,0xf5,0x05,0x11,0x11,0x1c,0x1f,0x0f},
	{0x78,0x00,0x01,0x11},
	{0x0A,0x00,0x01,0x29},
	{0x00,0x00,0x01,0x2c},
};

static struct lcm_cmds code_in_sleep[]={
	{0x78,0x00,0x01,0x28},
	{0x78,0x00,0x01,0x10},
};
static struct lcm_cmds code_out_sleep[]={
	{0x78,0x00,0x01,0x11},
	{0x0A,0x00,0x01,0x29},
};

static int32_t gc9304_init(struct panel_spec * self)
{
	static int32_t is_first_run = 1 ;
	uint32_t i = 0;
	spi_send_cmd_t spi_send_cmd = self->info.spi->ops->spi_send_cmd;
	printk("sprdfb:gc9304_init\n");
	if(is_first_run){
		gpio_request(LCM_GPIO_RSTN,"LCM_RSTN\n");
		gpio_direction_output(50,1);
		is_first_run = 0;
	}
	gpio_set_value(LCM_GPIO_RSTN,1);
	msleep(10);
	gpio_set_value(LCM_GPIO_RSTN,0);
	msleep(10);
	gpio_set_value(LCM_GPIO_RSTN,1);
	msleep(20);
	for(i = 0; i < ARRAY_SIZE(init_code); i++){
		spi_send_cmd(init_code[i].data);
		if(init_code[i].sleep)
			msleep(init_code[i].sleep);
	}
}
static int32_t gc9304_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	uint32_t i = 0;
	spi_send_cmd_t spi_send_cmd = self->info.spi->ops->spi_send_cmd;
	struct lcm_cmds *code;
	if(is_sleep)
		code = code_in_sleep;
	else
		code = code_out_sleep;
	for(i = 0; i < 2; i++){
		spi_send_cmd(code[i].data);
		if(code[i].sleep)
			msleep(code[i].sleep);
	}
}
static int32_t gc9304_set_window(struct panel_spec *self,
		uint16_t left, uint16_t top, uint16_t right, uint16_t bottom)
{
	uint32_t i = 0;
	spi_send_cmd_t spi_send_cmd = self->info.spi->ops->spi_send_cmd;
	struct lcm_cmds setwindow[]={
		{0x00,0x00,0x05,0x2a,left>>8,left&0xff,right>>8,right&0xff},
		{0x00,0x00,0x05,0x2b,top>>8,top&0xff,bottom>>8,bottom&0xff},
		{0x00,0x00,0x01,0x2c},
	};
	for(i = 0; i < ARRAY_SIZE(setwindow); i++){
		spi_send_cmd(setwindow[i].data);
		if(setwindow[i].sleep)
			msleep(setwindow[i].sleep);

	}
}
static int32_t gc9304_invalidate(struct panel_spec *self)
{
	printk("gc9304_invalidate\n");

	return self->ops->panel_set_window(self, 0, 0,
		self->width - 1, self->height - 1);
}
static int32_t gc9304_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom)
{
	printk("gc9304_invalidate_rect \n");
	return self->ops->panel_set_window(self, left, top,
			right, bottom);
}

static int32_t gc9304_read_id(struct panel_spec *self)
{
	return 0x9304;
}
static int32_t gc9304_reset(struct panel_spec *self){
	return 0;
}

static struct panel_operations lcd_gc9304_spi_operations = {
	.panel_init = gc9304_init,
	.panel_set_window = gc9304_set_window,
	.panel_invalidate_rect= gc9304_invalidate_rect,
	.panel_invalidate = gc9304_invalidate,
	.panel_enter_sleep = gc9304_enter_sleep,
	.panel_readid          = gc9304_read_id,
	.panel_reset		=gc9304_reset,
};

static struct info_spi lcd_gc9304_spi_info = {
	.bus_num = 0,
	.cs = 0,
	.cd_gpio = CONFIG_FB_SPI_CD_GPIO_NUM,
	.spi_mode = 1,
	.spi_pol_mode = SPI_MODE_0,
	.speed = GC9304_SPI_SPEED,
	.en_softbigendian = 0,
};

static struct panel_spec lcd_gc9304_spi_spec = {
	.width = 240,
	.height = 320,
	.bpp = 2,
	.fps = 35,
	.type = SPRDFB_PANEL_TYPE_SPI,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.spi = &lcd_gc9304_spi_info
	},
	.ops = &lcd_gc9304_spi_operations,
	.suspend_mode = SEND_SLEEP_CMD,
};
static struct panel_cfg lcd_gc9304_spi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x9304,
	.lcd_name = "lcd_gc9304_rgb_spi",
	.panel = &lcd_gc9304_spi_spec,
};
static int __init lcd_gc9304_spi_init(void)
{
	return sprdfb_panel_register(&lcd_gc9304_spi);
}

subsys_initcall(lcd_gc9304_spi_init);

