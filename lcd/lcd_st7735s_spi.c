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

#define ST7735S_SPI_SPEED        (24*1000*1000UL)


#define LCM_GPIO_RSTN		(50)
static struct lcm_cmds init_code[] = {
	{0x78,0x00,0x01,0x11},

	{0x00,0x00,0x04,0xb1,0x01,0x2c,0x2d},
	{0x00,0x00,0x04,0xb2,0x05,0x3a,0x3a},

	{0x00,0x00,0x07,0xb3,0x05,0x3a,0x3a,0x05,0x3a,0x3a},

	{0x00,0x00,0x02,0xb4,0x03},
	{0x00,0x00,0x04,0xc0,0x25,0x05,0x84},
	{0x00,0x00,0x02,0xc1,0xc0},
	{0x00,0x00,0x03,0xc2,0x0d,0x00},
	{0x00,0x00,0x03,0xc3,0x8d,0x2a},
	{0x00,0x00,0x03,0xc4,0x8d,0xee},
	{0x00,0x00,0x02,0xc5,0x0c},

	{0x00,0x00,0x02,0x36,0xc8},


	{0x00,0x00,0x11,0xe0,0x05,0x1a,0x0c,0x0e ,0x3a,0x34,0x2d,0x2f,0x2d ,0x2a,0x2f,0x3c,0x00,0x01 ,0x02,0x10},
	{0x00,0x00,0x11,0xe1,0x04,0x1b,0x0d,0x0e ,0x2d,0x29,0x24,0x29,0x28 ,0x26,0x31,0x3b,0x00,0x00 ,0x03,0x12},

	//{0x00,0x00,0x02,0xf0,0x01},
	//{0x00,0x00,0x02,0xf6,0x00},

	{0x00,0x00,0x02,0xfc,0x8c},

	{0x00,0x00,0x02,0x3a,0x05},
	{0x00,0x00,0x02,0x35,0x00},


	//{0x0A,0x00,0x01,0x29},

	//{0x00,0x00,0x05,0x2a,0x00,0x00,0x00,0x7f},
	//{0x00,0x00,0x05,0x2b,0x00,0x00,0x00,0x9f},
	//{0x78,0x00,0x01,0x11},
	{0x0A,0x00,0x01,0x29},
	{0x00,0x00,0x01,0x2c},

};

static struct lcm_cmds code_in_sleep[]={

	{0x78,0x00,0x01,0x28},
	{0x78,0x00,0x01,0x10},
};
static struct lcm_cmds code_out_sleep[]={

	{0x80,0x00,0x01,0x11},
	{0x0A,0x00,0x01,0x29},
};

static int32_t st7735s_init(struct panel_spec * self)
{
	static int32_t is_first_run = 1 ;
	uint32_t i = 0;
	spi_send_cmd_t spi_send_cmd = self->info.spi->ops->spi_send_cmd;
	printk("sprdfb:st7735s_init\n");
	if(is_first_run){
		gpio_request(LCM_GPIO_RSTN,"LCM_RSTN\n");
		gpio_direction_output(50,1);
		is_first_run = 0;
	}
	gpio_set_value(LCM_GPIO_RSTN,1);
	msleep(100);
	gpio_set_value(LCM_GPIO_RSTN,0);
	msleep(100);
	gpio_set_value(LCM_GPIO_RSTN,1);
	msleep(200);
	for(i = 0; i < ARRAY_SIZE(init_code); i++){
		spi_send_cmd(init_code[i].data);
		if(init_code[i].sleep)
			msleep(init_code[i].sleep);
	}
}
static int32_t st7735s_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	uint32_t i = 0;
	spi_send_cmd_t spi_send_cmd = self->info.spi->ops->spi_send_cmd;
	printk("sprdfb:st7735s_enter_sleep is_sleep = %d \n",is_sleep);
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
static int32_t st7735s_set_window(struct panel_spec *self,
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
static int32_t st7735s_invalidate(struct panel_spec *self)
{
	printk("st7735s_invalidate\n");

	return self->ops->panel_set_window(self, 0, 0,
		self->width - 1, self->height - 1);
}
static int32_t st7735s_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom)
{
	printk("st7735s_invalidate_rect \n");
	return self->ops->panel_set_window(self, left, top,
			right, bottom);
}

static int32_t st7735s_read_id(struct panel_spec *self)
{
	return 0x7735;
}
static int32_t st7735s_reset(struct panel_spec *self){
	return 0;
}

static struct panel_operations lcd_st7735s_spi_operations = {
	.panel_init = st7735s_init,
	.panel_set_window = st7735s_set_window,
	.panel_invalidate_rect= st7735s_invalidate_rect,
	.panel_invalidate = st7735s_invalidate,
	.panel_enter_sleep = st7735s_enter_sleep,
	.panel_readid          = st7735s_read_id,
	.panel_reset		=st7735s_reset,
};

static struct info_spi lcd_st7735s_spi_info = {
	.bus_num = 0,
	.cs = 0,
	.cd_gpio = 53,
	.spi_mode = 1,
	.spi_pol_mode = SPI_MODE_0,
	.speed = ST7735S_SPI_SPEED,
	.en_softbigendian = 0,
};

static struct panel_spec lcd_st7735s_spi_spec = {
	.width = 128,
	.height = 160,
	.bpp = 2,
	.fps = 50,
	.type = SPRDFB_PANEL_TYPE_SPI,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.spi = &lcd_st7735s_spi_info
	},
	.ops = &lcd_st7735s_spi_operations,
	.suspend_mode = SEND_SLEEP_CMD,
};
static struct panel_cfg lcd_st7735s_spi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x7735,
	.lcd_name = "lcd_st7735s_spi",
	.panel = &lcd_st7735s_spi_spec,
};
static int __init lcd_st7735s_spi_init(void)
{
	return sprdfb_panel_register(&lcd_st7735s_spi);
}

subsys_initcall(lcd_st7735s_spi_init);

