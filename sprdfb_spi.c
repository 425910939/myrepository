/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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
#ifdef CONFIG_OF
#include <linux/fb.h>
#endif
#include <soc/sprd/sci.h>
#include "sprdfb.h"
#include "sprdfb_panel.h"
#include "sprdfb_spi_intf.h"


void spi_write_data(struct spi_transfer* xfer)
 {
	struct spi_device *spi_dev = spi_intf->spi_dev;
	struct panel_spec* panel = spi_intf->panel;
	struct spi_message msg;
	xfer->gpio_cd_level = 0x81;
	spi_message_init(&msg);
	spi_message_add_tail(xfer, &msg);
	return spi_sync(spi_dev, &msg);

 }
void spi_write_cmd(struct spi_transfer* xfer)
{
	struct spi_device *spi_dev = spi_intf->spi_dev;
	struct panel_spec* panel = spi_intf->panel;
	struct spi_message msg;
	xfer->gpio_cd_level = 0x80;
	spi_message_init(&msg);
	spi_message_add_tail(xfer, &msg);
	return spi_sync(spi_dev, &msg);
}

 void spi_send_cmds(uint8_t* data)
{
	uint16_t spi_data;
	uint16_t len = 0;
	bool cd_bit = false;
	uint16_t i = 0;
	uint16_t cd_gpio = spi_intf->panel->info.spi->cd_gpio;
	struct spi_transfer xfer;
	memset(&xfer,0,sizeof(xfer));
	len = data[0] ;
	len =( len<< 8) | data[1];
	if(cd_gpio == 0)
		cd_bit = true;

	if(!cd_bit){
		xfer.len = 1;
		xfer.tx_buf = &(data[2]);
		xfer.bits_per_word = 8;
		spi_write_cmd(&xfer);
	}
	else{
		spi_data = data[2];
		xfer.len = 2;
		xfer.tx_buf = &spi_data;
		xfer.bits_per_word = 9;
		spi_write_cmd(&xfer);
	} 
	if(len == 1)
		return;
	for(;i < len - 1;i++){
		if(cd_bit){
			spi_data = (data[i+2] ) | ( cd_bit << 8);
			xfer.len = 2;
			xfer.tx_buf = &spi_data;
			xfer.bits_per_word = 9;
			spi_write_data(&xfer);
		}
		else {
			xfer.len = len-1;
			xfer.tx_buf = &(data[3]) ;
			xfer.bits_per_word = 8;
			spi_write_data(&xfer);
			break;
		}
	}
}

 void spi_send_datas(uint8_t* data)
{
	return;
}

 void spi_read_datas( uint8_t* data)
{
	return;
}


bool sprdfb_spi_init(struct sprdfb_device *dev)
{

	dev->panel->info.spi->spi_intf = spi_intf;
	sprd_register_spi_intf_client(dev->panel);

	return 0;
}

bool sprdfb_spi_uninit(struct sprdfb_device *dev)
{
	return true;
}

struct ops_spi sprdfb_spi_ops = {

	.spi_send_cmd = spi_send_cmds,
	.spi_send_data = spi_send_datas,
	.spi_read = spi_read_datas,

};

static uint32_t spi_readid(struct panel_spec *self)
{

	uint32_t id = 0;
	struct info_spi *spi = self->info.spi;
	/* default id reg is 0 */
	{
		spi->ops->spi_send_cmd(0x0);
		spi->ops->spi_read(&id);
	}
	return id;

}

static int32_t sprdfb_spi_panel_check(struct panel_spec *panel)
{
	if(NULL == panel){
		printk("sprdfb: [%s] fail. (Invalid param)\n", __FUNCTION__);
		return false;
	}

	if(SPRDFB_PANEL_TYPE_SPI != panel->type ) {
		printk("sprdfb: [%s] fail. (not spi or lvds param)\n", __FUNCTION__);
		return false;
	}

	pr_debug("sprdfb: [%s]\n",__FUNCTION__);

	return true;
}

static void sprdfb_spi_panel_mount(struct sprdfb_device *dev)
{

	if((NULL == dev) || (NULL == dev->panel)){
		printk(KERN_ERR "sprdfb: [%s]: Invalid Param\n", __FUNCTION__);
		return;
	}

	pr_debug(KERN_INFO "sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);

	printk("sprdfb: sprdfb_spi_panel_mount \n");

	if (dev->panel->type == SPRDFB_PANEL_TYPE_SPI)
	{
		dev->panel_if_type = SPRDFB_PANEL_IF_SPI;
		printk("sprdfb:sprdfb_spi_panel_mount sprdfb_spi_ops\n");
		dev->panel->info.spi->ops = &sprdfb_spi_ops;
		dev->panel->info.spi->spi_intf = spi_intf;
		spi_intf->panel=dev->panel;
		spi_intf->intf_config(dev->panel);
	}

	if(NULL == dev->panel->ops->panel_readid){
		dev->panel->ops->panel_readid = spi_readid;
	}

}

bool sprdfb_spi_panel_init(struct sprdfb_device *dev)
{
	sprd_register_spi_intf_client(dev->panel);
	return true;

}

static void sprdfb_spi_panel_uninit(struct sprdfb_device *dev)
{
	return;
}

struct panel_if_ctrl sprdfb_spi_ctrl = {
	.if_name		= "spi",
	.panel_if_check	= sprdfb_spi_panel_check,
	.panel_if_mount	= sprdfb_spi_panel_mount,
	.panel_if_init		= sprdfb_spi_panel_init,
	.panel_if_uninit		= sprdfb_spi_panel_uninit,
	.panel_if_before_refresh	= NULL,
	.panel_if_after_refresh	= NULL,
};

