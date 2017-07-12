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

#define ST7789V_SPI_SPEED        (48*1000*1000UL)

#define LCM_GPIO_RSTN		(50)
static struct lcm_cmds init_code[] = {
	{0x78,0x00,0x01,0x11},
	
	{0x00,0x00,0x02,0x36,0x00},
	{0x00,0x00,0x02,0x3a,0x05},
	{0x00,0x00,0x02,0x35,0x00},
	//-----st7789s frame rate setting
	{0x00,0x00,0x06,0xb2,0x0c,0x0c,0x00,0x33,0x33},
	{0x00,0x00,0x02,0xb7,0x35},
	
	//-----st7789s power setting
	{0x00,0x00,0x02,0xbb,0x36},
	{0x00,0x00,0x02,0xc0,0x2c},
	{0x00,0x00,0x02,0xc2,0x01},
	{0x00,0x00,0x02,0xc3,0x0c},
	{0x00,0x00,0x02,0xc4,0x20},
	{0x00,0x00,0x02,0xc6,0x0f},

	{0x00,0x00,0x03,0xd0,0xa4,0xa1},
	
	//----st7789s gamma setting
	{0x00,0x00,0x0f,0xe0,0xd0,0x00,0x0e,0x0e,0x15,0x0d,0x37,0x43,0x46,0x07,0x10,0x12,0x18,0x19},
	{0x00,0x00,0x0f,0xe1,0xd0,0x00,0x05,0x0d,0x0c,0x06,0x2d,0x44,0x3f,0x0c,0x18,0x17,0x18,0x19},
	

	{0x00,0x00,0x02,0x35,0x00},
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

static int32_t is_first_run = 1 ;

static int32_t st7789v_init(struct panel_spec * self)
{
	uint32_t i = 0,j = 0;
	spi_send_cmd_t spi_send_cmd = self->info.spi->ops->spi_send_cmd;
	printk("sprdfb:st7789v_init\n");
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
static int32_t st7789v_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
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
static int32_t st7789v_set_window(struct panel_spec *self,
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
static int32_t st7789v_invalidate(struct panel_spec *self)
{
	printk("st7789v_invalidate\n");

	return self->ops->panel_set_window(self, 0, 0,
		self->width - 1, self->height - 1);
}
static int32_t st7789v_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom)
{
	printk("st7789v_invalidate_rect \n");
	return self->ops->panel_set_window(self, left, top,
			right, bottom);
}

static int32_t st7789v_read_id(struct panel_spec *self)
{
	return 0x7789;
}
static int32_t st7789v_reset(struct panel_spec *self){
	return 0;
}

static struct panel_operations lcd_st7789v_spi_operations = {
	.panel_init = st7789v_init,
	.panel_set_window = st7789v_set_window,
	.panel_invalidate_rect= st7789v_invalidate_rect,
	.panel_invalidate = st7789v_invalidate,
	.panel_enter_sleep = st7789v_enter_sleep,
	.panel_readid          = st7789v_read_id,
	.panel_reset		=st7789v_reset,
};

static struct info_spi lcd_st7789v_spi_info = {
	.bus_num = 0,
	.cs = 0,
	.cd_gpio = CONFIG_FB_SPI_CD_GPIO_NUM,
	.spi_mode = 1,
	.spi_pol_mode = SPI_MODE_0,
	.speed = ST7789V_SPI_SPEED,
	.en_softbigendian = 0,
};

static struct panel_spec lcd_st7789v_spi_spec = {
	.width = 240,
	.height = 320,
	.bpp = 2,
	.fps = 35,
	.type = SPRDFB_PANEL_TYPE_SPI,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.spi = &lcd_st7789v_spi_info
	},
	.ops = &lcd_st7789v_spi_operations,
	.suspend_mode = SEND_SLEEP_CMD,
};
static struct panel_cfg lcd_st7789v_spi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x7789,
	.lcd_name = "lcd_st7789v_rgb_spi",
	.panel = &lcd_st7789v_spi_spec,
};
static int __init lcd_st7789v_spi_init(void)
{
	return sprdfb_panel_register(&lcd_st7789v_spi);
}

subsys_initcall(lcd_st7789v_spi_init);

