#include "sprdfb_spi_intf.h"
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	early_suspend;
#endif
struct sprd_spi_intf *spi_intf;
static unsigned char sprdfb_refresh_cmd[]={0x2a,0x2b,0x2c};
static unsigned char sprdfb_refresh_data[][4] = {{0x00,0x00,0x00,0xef},{0x00,0x00,0x01,0x3f}};
static struct spi_transfer sprdfb_refresh_transfers[] = {
	{
		.len = 1,
		.tx_buf=&sprdfb_refresh_cmd[0],
		.gpio_cd_level=0x80,
		.bits_per_word = 8,
	},
	{
		.len = 4,
		.tx_buf=&sprdfb_refresh_data[0][0],
		.gpio_cd_level=0x81,
		.bits_per_word = 8,
	},
	{
		.len = 1,
		.tx_buf=&sprdfb_refresh_cmd[1],
		.gpio_cd_level=0x80,
		.bits_per_word = 8,
	},
	{
		.len = 4,
		.tx_buf=&sprdfb_refresh_data[1][0],
		.gpio_cd_level=0x81,
		.bits_per_word = 8,
	},
	{
		.len = 1,
		.tx_buf=&sprdfb_refresh_cmd[2],
		.gpio_cd_level=0x80,
		.bits_per_word = 8,
	},
};
static struct spi_message sprdfb_refresh_msg;
static struct spi_transfer sprdfb_refresh_xfer = {
	.len = 240*320*2,
	.gpio_cd_level = 0x81,
	.bits_per_word = 32,
};

static void sprdfb_spi_complete(void *context)
{
	return complete_all(&spi_intf->spi_complete);
}

static int sprdfb_spi_transfer(void *base)
{
	int i;
	spi_message_init(&sprdfb_refresh_msg);
	sprdfb_refresh_msg.complete =sprdfb_spi_complete;
	for(i = 0 ; i < 5; i ++)
		spi_message_add_tail(&sprdfb_refresh_transfers[i], &sprdfb_refresh_msg);
	sprdfb_refresh_xfer.tx_buf=base;
	spi_message_add_tail(&sprdfb_refresh_xfer,&sprdfb_refresh_msg);
	return spi_async(spi_intf->spi_dev, &sprdfb_refresh_msg);
}
int sprdfb_spi_refresh(void *base)
{
	int ret ;
	if(!completion_done(&spi_intf->spi_complete)){
		ret = wait_for_completion_timeout(&spi_intf->spi_complete,msecs_to_jiffies(200));
		switch(ret)
		{
			case 0://time out
				printk("sprdfb:sprdfb_spi_refresh timeout\n");
				//try_wait_for_completion(&spi_intf->spi_complete);
				return -2;
				break;
			case -ERESTARTSYS://interrupt
				printk("sprdfb:sprdfb_spi_refresh interruptible -ERESTARTSYS\n");
				try_wait_for_completion(&spi_intf->spi_complete);
				return ret;
				break;
			default://normal
				break;
		}
	}
	INIT_COMPLETION(spi_intf->spi_complete);
	spi_intf->current_base = base;
	ret = sprdfb_spi_transfer(spi_intf->current_base);
	if(ret){
		printk("sprdfb:sprdfb_spi_refresh spi_async error\n");
	}
	return ret;
}
static int sprd_spi_intf_config(struct panel_spec *panel)
{
	uint16_t width, height;
	printk("sprdfb:sprd_spi_intf_config\n");
	width=panel->width;
	height=panel->height;
	spi_intf->refresh_len = width*height*2;
	sprdfb_refresh_xfer.len=spi_intf->refresh_len;
	sprdfb_refresh_data[0][2]=((width-1)>>8)&0xff;
	sprdfb_refresh_data[0][3]=(width-1)&0xff;
	sprdfb_refresh_data[1][2]=((height-1)>>8)&0xff;
	sprdfb_refresh_data[1][3]=(height-1)&0xff;
	init_completion(&spi_intf->spi_complete);
	spi_intf->spi_complete.done = 1;
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sprd_spi_intf_suspend(struct early_suspend *handler)
{
	printk("%s:\n", __func__);
	if(!completion_done(&spi_intf->spi_complete))
		try_wait_for_completion(&spi_intf->spi_complete);
	spi_intf->panel->ops->panel_enter_sleep(spi_intf->panel,1);
}

static void sprd_spi_intf_resume(struct early_suspend *handler)
{
	printk("%s:\n", __func__);
	spi_intf->spi_complete.done = 1;
	spi_intf->panel->ops->panel_init(spi_intf->panel);
}
#endif

static int sprd_spi_intf_probe(struct spi_device *spi_dev) {
	int ret = 0;
	spi_intf->spi_dev = spi_dev;
	spi_setup(spi_dev);
	device_init_wakeup(&spi_dev->dev, 1);
#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend.suspend = sprd_spi_intf_suspend;
	early_suspend.resume  = sprd_spi_intf_resume;
	early_suspend.level   = EARLY_SUSPEND_LEVEL_DISABLE_FB+1;
	register_early_suspend(&early_suspend);
#endif
	spi_set_drvdata(spi_dev, spi_intf);
	return ret;
}

static int sprd_spi_intf_remove(struct spi_device *spi_dev) {
	spi_intf->spi_dev= NULL;
	spi_set_drvdata(spi_dev, NULL);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&early_suspend);
#endif
	return 0;
}
struct spi_board_info spi_board = {
		.modalias = "spi_intf",
	};

struct spi_driver spi_intf_drv = {
	.driver = {
		.name =	"spi_intf",
		.owner = THIS_MODULE,
	},
	.probe = sprd_spi_intf_probe,
	.remove = sprd_spi_intf_remove,
};


static int sprd_spi_intf_register_driver(void)
{
	int ret;
	printk("%s: start\n", __func__);
	//sci_glb_set((SPRD_APBREG_BASE + 0x0004),BIT(5));//spi0 reset
	//sci_glb_clr((SPRD_APBREG_BASE + 0x0004),BIT(5));
	ret = spi_register_driver(&spi_intf_drv);
	printk("%s: end.\n", __func__);

	return ret;
}

static void sprd_spi_intf_unregister_driver(void)
{
	spi_unregister_driver(&spi_intf_drv);
}

/*---------------------------------- module ------------------------------------*/
static int __init sprd_spi_intf_init(void) {
	int ret;
	spi_intf = kmalloc(sizeof(struct sprd_spi_intf), GFP_KERNEL);
	if ( spi_intf==NULL) {
		printk("%s: spi_intf kmalloc failed.\n", __func__);
		if(spi_intf!=NULL)
			kfree(spi_intf);
		return -ENOMEM;
	}
	spi_intf->intf_config = sprd_spi_intf_config;
	ret = sprd_spi_intf_register_driver();
	if (ret < 0) {
		printk("%s: spi_register_driver failed. ret=%d\n", __func__, ret);
	}
	return ret;
}

static void __exit sprd_spi_intf_exit(void) {
	sprd_spi_intf_unregister_driver();
	if(spi_intf!=NULL)
		kfree(spi_intf);
}

int sprd_register_spi_intf_client(struct panel_spec *panel)
{
	static int is_first_run = 1;
	int ret;
	if(is_first_run == 1){
		struct info_spi *hwinfo =panel->info.spi;
		spi_board.bus_num = hwinfo->bus_num;
		spi_board.max_speed_hz = hwinfo->speed;
		spi_board.mode = hwinfo->spi_pol_mode;
		spi_board.chip_select = hwinfo->cs;
		ret = spi_register_board_info(&spi_board,1);
		if(ret < 0)
			return ret;
		is_first_run = 0;
	}
	return 0;
}

module_init(sprd_spi_intf_init);
module_exit(sprd_spi_intf_exit);

