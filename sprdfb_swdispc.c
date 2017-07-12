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
#define pr_fmt(fmt) "sprdfb: " fmt

#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/fb.h>
#include <linux/delay.h>
#if (defined(CONFIG_SPRD_SCXX30_DMC_FREQ) || defined(CONFIG_SPRD_SCX35_DMC_FREQ))
#include <linux/devfreq.h>
#endif
#include<linux/gpio.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#ifdef CONFIG_OF
#include <linux/of_irq.h>
#endif
#include <asm/cacheflush.h>
#include "sprdfb_panel.h"
#include "sprdfb.h"
#include "sprdfb_chip_common.h"
#ifdef CONFIG_CPU_IDLE
#include <soc/sprd/cpuidle.h>
#endif
#include <linux/dma-mapping.h>

#define SWIDPSC_GPIO_TE		(51)

struct sprdfb_swdispc_context {
	bool			is_inited;
	bool			is_first_frame;
	struct sprdfb_device	*dev;

	uint32_t	 	vsync_waiter;
	wait_queue_head_t		vsync_queue;
	uint32_t	        vsync_done;

#ifdef  CONFIG_FB_LCD_OVERLAY_SUPPORT
	/* overlay */
	uint32_t  overlay_state;  /*0-closed, 1-configed, 2-started*/
//	struct semaphore   overlay_lock;
#endif

#ifdef CONFIG_FB_VSYNC_SUPPORT
	uint32_t	 	waitfor_vsync_waiter;
	wait_queue_head_t		waitfor_vsync_queue;
	uint32_t	        waitfor_vsync_done;
#endif
#ifdef CONFIG_FB_MMAP_CACHED
	struct vm_area_struct *vma;
#endif
};
#ifdef CONFIG_OF
unsigned long g_dispc_base_addr = 0;
EXPORT_SYMBOL(g_dispc_base_addr);
#endif

static struct sprdfb_swdispc_context swdispc_ctx = {0};

extern unsigned long lcd_base_from_uboot;

extern void sprdfb_panel_suspend(struct sprdfb_device *dev);
extern void sprdfb_panel_shutdown(struct sprdfb_device *dev);
extern void sprdfb_panel_resume(struct sprdfb_device *dev, bool from_deep_sleep);
extern void sprdfb_panel_before_refresh(struct sprdfb_device *dev);
extern void sprdfb_panel_after_refresh(struct sprdfb_device *dev);
extern void sprdfb_panel_invalidate(struct panel_spec *self);
extern void sprdfb_panel_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom);
extern int sprdfb_spi_refresh(void* base);
extern int sprdfb_spi_logo_proc(unsigned long addr);

static int32_t sprdfb_swdispc_init(struct sprdfb_device *dev);
static void swdispc_stop(struct sprdfb_device *dev);
static void swdispc_module_enable(void);

static irqreturn_t swdispc_isr(int irq, void *data)
{
	struct sprdfb_swdispc_context *swdispc_ctx = (struct sprdfb_swdispc_context *)data;
	struct sprdfb_device *dev = swdispc_ctx->dev;
	if(NULL == dev){
		return IRQ_HANDLED;
	}
	swdispc_ctx->waitfor_vsync_done = 1;
	if(swdispc_ctx->waitfor_vsync_waiter){
		wake_up_interruptible_all(&(swdispc_ctx->waitfor_vsync_queue));
	}
	return IRQ_HANDLED;
}
/* swdispc soft reset */

static int32_t swdispc_sync(struct sprdfb_device *dev)
{
	int ret;
	if (dev->enable == 0) {
		printk("sprdfb: swdispc_sync fb suspeneded already!!\n");
		return -1;
	}

#ifdef CONFIG_SC_FPGA
	ret = wait_event_interruptible_timeout(swdispc_ctx.vsync_queue,
			          swdispc_ctx.vsync_done, msecs_to_jiffies(500));
#else
	ret = wait_event_interruptible_timeout(swdispc_ctx.vsync_queue,
			          swdispc_ctx.vsync_done, msecs_to_jiffies(100));
#endif

	if (!ret) { /* time out */
		swdispc_ctx.vsync_done = 1; /*error recovery */
		printk(KERN_ERR "sprdfb: swdispc_sync time out  !!!!!\n");
		return -1;
	}
	return 0;
}


static void swdispc_run(struct sprdfb_device *dev)
{
	void* base;
	if(0 == dev->enable){
		return;
	}
		/* start refresh */
	swdispc_ctx.vsync_done = 0;

	if(dev->gsp_proc){
		base = dev->gsp_buffer_addr_f;
		dev->gsp_proc = false;
	}
	else
		base = dev->fb->screen_base + dev->fb->fix.line_length * dev->fb->var.yoffset;
	sprdfb_spi_refresh(base);
	swdispc_ctx.vsync_done = 1;
	if (swdispc_ctx.vsync_waiter) {
		wake_up_interruptible_all(&(swdispc_ctx.vsync_queue));
		swdispc_ctx.vsync_waiter = 0;
	}
}

static void swdispc_stop(struct sprdfb_device *dev)
{
	if(SPRDFB_PANEL_IF_DPI == dev->panel_if_type){
		/*dpi register update with SW only*/
		swdispc_ctx.is_first_frame = true;
	}
}

static int32_t sprdfb_swdispc_uninit(struct sprdfb_device *dev)
{
	pr_debug(KERN_INFO "sprdfb: [%s]\n",__FUNCTION__);
	dev->enable = 0;
	return 0;
}

static int32_t sprdfb_swdispc_module_init(struct sprdfb_device *dev)
{
	int ret = 0;
	int irq_num = 0;

	if(swdispc_ctx.is_inited){
		printk(KERN_WARNING "sprdfb: swdispc_module has already initialized! warning!!");
		return 0;
	}
	else{
		printk(KERN_INFO "sprdfb: swdispc_module_init. call only once!");
	}
	swdispc_ctx.vsync_done = 1;
	swdispc_ctx.vsync_waiter = 0;
	init_waitqueue_head(&(swdispc_ctx.vsync_queue));
#ifdef CONFIG_FB_VSYNC_SUPPORT
	swdispc_ctx.waitfor_vsync_done = 1;
	swdispc_ctx.waitfor_vsync_waiter = 0;
	init_waitqueue_head(&(swdispc_ctx.waitfor_vsync_queue));
#endif
	sema_init(&dev->refresh_lock, 1);

	gpio_request(SWIDPSC_GPIO_TE, "SWIDPSC_GPIO_TE");
	gpio_direction_input(SWIDPSC_GPIO_TE);
	irq_num = gpio_to_irq(SWIDPSC_GPIO_TE);
	ret = request_irq(irq_num, swdispc_isr, IRQF_DISABLED | IRQF_TRIGGER_FALLING, "SW-DISPC", &swdispc_ctx);
	if (ret) {
		printk(KERN_ERR "sprdfb: swdispc failed to request irq!\n");
		return -1;
	}
	swdispc_ctx.is_inited = true;

	return 0;

}

static int32_t sprdfb_swdispc_early_init(struct sprdfb_device *dev)
{
	int ret = 0;


	if(!swdispc_ctx.is_inited){
		//init
		if(dev->panel_ready){
			//panel ready
			printk(KERN_INFO "sprdfb: [%s]: swdispc has alread initialized\n", __FUNCTION__);
			swdispc_ctx.is_first_frame = false;
		}else{
			//panel not ready
			printk(KERN_INFO "sprdfb: [%s]: swdispc is not initialized\n", __FUNCTION__);

			swdispc_ctx.is_first_frame = true;
		}
		ret = sprdfb_swdispc_module_init(dev);
	}else{
		//resume
		printk(KERN_INFO "sprdfb: [%s]: sprdfb_swdispc_early_init resume\n", __FUNCTION__);
		//swdispc_reset();
		//swdispc_module_enable();
		swdispc_ctx.is_first_frame = true;
	}

	return ret;
}

static int32_t sprdfb_swdispc_init(struct sprdfb_device *dev)
{
	uint32_t swdispc_int_en_reg_val = 0x00;//ONLY for swdispc interrupt en reg
	pr_debug(KERN_INFO "sprdfb: [%s]\n",__FUNCTION__);

	if(NULL == dev){
            printk("sprdfb: [%s] Invalid parameter!\n", __FUNCTION__);
            return -1;
	}

	swdispc_ctx.dev = dev;
	dev->enable = 1;

	return 0;
}

static int32_t sprdfb_swdispc_refresh (struct sprdfb_device *dev)
{
	printk("sprdfb: [%s]\n",__FUNCTION__);

	down(&dev->refresh_lock);
	if(0 == dev->enable){
		printk("sprdfb: [%s]: do not refresh in suspend!!!\n", __FUNCTION__);
		goto ERROR_REFRESH;
	}

	if(SPRDFB_PANEL_IF_SPI == dev->panel_if_type){
		swdispc_ctx.vsync_waiter ++;
		swdispc_sync(dev);
	}

	dev->frame_count += 1;
	swdispc_run(dev);

ERROR_REFRESH:
	up(&dev->refresh_lock);

	if(NULL != dev->logo_buffer_addr_v){
		printk("sprdfb: free logo proc buffer!\n");
		free_pages((unsigned long)dev->logo_buffer_addr_v, get_order(dev->logo_buffer_size));
		dev->logo_buffer_addr_v = 0;
	}

	return 0;
}

static int32_t sprdfb_swdispc_shutdown(struct sprdfb_device *dev)
{
	return 0;
}

static int32_t sprdfb_swdispc_suspend(struct sprdfb_device *dev)
{
	printk(KERN_INFO "sprdfb: [%s], dev->enable = %d\n",__FUNCTION__, dev->enable);

	if (0 != dev->enable){
		down(&dev->refresh_lock);
		if(SPRDFB_PANEL_IF_SPI == dev->panel_if_type){
			/* must wait ,swdispc_sync() */
			if(swdispc_ctx.vsync_waiter != 0)
				swdispc_sync(dev);
			printk(KERN_INFO "sprdfb: [%s] got sync\n",__FUNCTION__);
		}
		dev->enable = 0;
		up(&dev->refresh_lock);
		swdispc_stop(dev);
		msleep(50); /*fps>20*/
	}else{
		printk(KERN_ERR "sprdfb: [%s]: Invalid device status %d\n", __FUNCTION__, dev->enable);
	}
	return 0;
}
static int32_t sprdfb_swdispc_resume(struct sprdfb_device *dev)
{
	printk(KERN_INFO "sprdfb: [%s], dev->enable= %d\n",__FUNCTION__, dev->enable);

	if (dev->enable == 0) {
		swdispc_ctx.vsync_done = 1;
		printk(KERN_INFO "sprdfb: [%s] from deep sleep\n",__FUNCTION__);
		sprdfb_swdispc_early_init(dev);
		sprdfb_swdispc_init(dev);
		dev->enable = 1;
	}
	printk(KERN_INFO "sprdfb: [%s], leave dev->enable= %d\n",__FUNCTION__, dev->enable);
	return 0;
}


#ifdef CONFIG_FB_VSYNC_SUPPORT
static int32_t spdfb_swdispc_wait_for_vsync(struct sprdfb_device *dev)
{
	int ret = 0;
	pr_debug("sprdfb: [%s]\n", __FUNCTION__);
	swdispc_ctx.waitfor_vsync_done = 0;
	swdispc_ctx.waitfor_vsync_waiter++;
	ret  = wait_event_interruptible_timeout(swdispc_ctx.waitfor_vsync_queue,
			swdispc_ctx.waitfor_vsync_done, msecs_to_jiffies(100));

	if (!ret) { /* time out */
		swdispc_ctx.waitfor_vsync_done = 1;
		printk(KERN_ERR "sprdfb: vsync time out!!!!!\n");
	}

	swdispc_ctx.waitfor_vsync_waiter = 0;
	pr_debug("sprdfb: [%s] (%d)\n", __FUNCTION__, ret);
	return 0;
}
#endif


static int32_t sprdfb_swdispc_refresh_logo (unsigned long addr)
{
	return 0;
}

void sprdfb_swdispc_logo_proc(struct sprdfb_device *dev)
{
	//inline size_t roundUpToPageSize(size_t x) {    return (x + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);}
	unsigned long logo_dst_p = 0;
	void *logo_src_v = NULL;
	void *logo_dst_v = NULL; //use the second frame buffer	,virtual
	uint32_t logo_size = 0;// should be rgb565

	pr_debug("sprdfb: %s[%d] enter.\n",__func__,__LINE__);

	if(dev == NULL) {
		printk("sprdfb: %s[%d]: dev == NULL, return without process logo!!\n",__func__,__LINE__);
		return;
	}

	if(lcd_base_from_uboot == 0L) {
		printk("sprdfb: %s[%d]: lcd_base_from_uboot == 0, return without process logo!!\n",__func__,__LINE__);
		return;
	}
	if(SPRDFB_PANEL_IF_DPI != dev->panel_if_type)
		return;

//#define USE_OVERLAY_BUFF
#ifdef CONFIG_FB_LOW_RES_SIMU
	if((0 != dev->display_width) && (0 != dev->display_height)){
		logo_size = dev->display_width * dev->display_height * 2;// should be rgb565
	}else
#endif
	logo_size = dev->panel->width * dev->panel->height * 2;// should be rgb565
#if 0
#ifndef USE_OVERLAY_BUFF
	kernel_fb_size = dev->panel->width * dev->panel->height * (dev->bpp / 8);
	kernel_fb_size = 0;
	logo_dst_v = dev->fb->screen_base + kernel_fb_size;//use the second frame buffer
	logo_dst_p = dev->fb->fix.smem_start + kernel_fb_size;//use the second frame buffer
#else
	logo_dst_p = SPRD_ION_OVERLAY_BASE-logo_size;//use overlay frame buffer
	logo_dst_v =  (uint32_t)ioremap(logo_dst_p, logo_size);
#endif
#else
	dev->logo_buffer_size = logo_size;
	dev->logo_buffer_addr_v = (void*)__get_free_pages(GFP_ATOMIC | __GFP_ZERO , get_order(logo_size));
	if (!dev->logo_buffer_addr_v) {
		printk(KERN_ERR "sprdfb: %s Failed to allocate logo proc buffer\n", __FUNCTION__);
		return;
	}
	printk(KERN_INFO "sprdfb: got %d bytes logo proc buffer at 0x%p\n", logo_size,
		dev->logo_buffer_addr_v);

	logo_dst_v = dev->logo_buffer_addr_v;
	logo_dst_p = __pa(dev->logo_buffer_addr_v);

#endif
	logo_src_v =  ioremap(lcd_base_from_uboot, logo_size);
	if (!logo_src_v || !logo_dst_v) {
		printk(KERN_ERR "sprdfb: %s[%d]: Unable to map boot logo memory: src-0x%p, dst-0x%p\n", __func__,
		    __LINE__,logo_src_v, logo_dst_v);
		return;
	}

	printk("sprdfb: %s[%d]: lcd_base_from_uboot: 0x%lx, logo_src_v:0x%p\n",__func__,__LINE__,lcd_base_from_uboot,logo_src_v);
	printk("sprdfb: %s[%d]: logo_dst_p:0x%lx,logo_dst_v:0x%p\n",__func__,__LINE__,logo_dst_p,logo_dst_v);
	memcpy(logo_dst_v, logo_src_v, logo_size);
	dma_sync_single_for_device(dev->fb->dev, logo_dst_p, logo_size, DMA_TO_DEVICE);

	iounmap(logo_src_v);

#if 0
#ifdef USE_OVERLAY_BUFF
	iounmap(logo_dst_v);
#endif
#endif
	sprdfb_swdispc_refresh_logo(logo_dst_p);

}

static int32_t sprdfb_swdispc_display_overlay(struct sprdfb_device *dev, struct overlay_display* setting)
{
#ifndef CONFIG_FB_LOW_RES_SIMU
	struct overlay_rect* rect = &(setting->rect);
	uint32_t size =( (rect->h << 16) | (rect->w & 0xffff));
#else
	uint32_t size = (dev->panel->width &0xffff) | ((dev->panel->height)<<16);
#endif
	swdispc_ctx.dev = dev;

	pr_debug("sprdfb: sprdfb_dispc_display_overlay: layer:%d, (%d, %d,%d,%d)\n",
		setting->layer_index, setting->rect.x, setting->rect.y, setting->rect.h, setting->rect.w);

	down(&dev->refresh_lock);
	if(0 == dev->enable){
		printk("sprdfb: [%s] leave (Invalid device status)!\n", __FUNCTION__);
		goto ERROR_DISPLAY_OVERLAY;
	}
	if(SPRDFB_PANEL_IF_DPI != dev->panel_if_type){
		swdispc_ctx.vsync_waiter ++;
		swdispc_sync(dev);
		//dispc_ctx.vsync_done = 0;

	}
	pr_debug(KERN_INFO "srpdfb: [%s] got sync\n", __FUNCTION__);

	swdispc_ctx.dev = dev;

	dev->frame_count += 1;
	swdispc_run(dev);

	if((SPRD_OVERLAY_DISPLAY_SYNC == setting->display_mode) && (SPRDFB_PANEL_IF_DPI != dev->panel_if_type)){
		swdispc_ctx.vsync_waiter ++;
		if (swdispc_sync(dev) != 0) {/* time out??? disable ?? */
			printk("sprdfb: sprdfb  do sprd_lcdc_display_overlay  time out!\n");
		}
		//dispc_ctx.vsync_done = 0;
	}

ERROR_DISPLAY_OVERLAY:
	up(&dev->refresh_lock);
	return 0;
}

struct overlay_ioremap_info {
       unsigned long phy_addr;
       unsigned long ioremap_addr;
};

static struct overlay_ioremap_info iio[2];

unsigned long get_overlay_ioremap_addr(unsigned long phy_addr) {
       int i;
retry:
      for (i=0; i<2; i++) {
               if (iio[i].phy_addr == phy_addr) {
                       return iio[i].ioremap_addr;
               }
       }

       int success = 0;
       for (i=0; i<2; i++) {
               if (!iio[i].phy_addr) {
                       iio[i].ioremap_addr = ioremap(phy_addr, 240*320*2);
                      iio[i].phy_addr = phy_addr;
                      printk(">>>> remap phy_addr = %p, ioremap addr = %p\n", phy_addr, iio[i].ioremap_addr);
                       success = 1;
                       break;
               }
       }

       if (!success) {
               printk(">>>> cant' get addr, phy_addr = %p\n", phy_addr);
               return NULL;
       }

       goto retry;
               
}

static int swdispc_overlay_img_configure(struct sprdfb_device *dev, int type, overlay_rect *rect, unsigned char *buffer, int y_endian, int uv_endian, bool rb_switch)
{
	uint32_t reg_value;

	pr_debug("sprdfb: [%s] : %d, (%d, %d,%d,%d), 0x%x\n", __FUNCTION__, type, rect->x, rect->y, rect->h, rect->w, (unsigned int)buffer);

	dev->gsp_buffer_addr_f = get_overlay_ioremap_addr((unsigned long)buffer);
	dev->gsp_proc = true;
	return 0;
}


/*TO DO: need mutext with suspend, resume*/
static int32_t sprdfb_swdispc_enable_overlay(struct sprdfb_device *dev, struct overlay_info* info, int enable)
{
	int result = -1;
	bool	is_refresh_lock_down=false;


	pr_debug("sprdfb: [%s]: %d, %d\n", __FUNCTION__, enable,  dev->enable);

	if(enable){  /*enable*/
		if(NULL == info){
			printk(KERN_ERR "sprdfb: sprdfb_dispc_enable_overlay fail (Invalid parameter)\n");
			goto ERROR_ENABLE_OVERLAY;
		}

		down(&dev->refresh_lock);
		is_refresh_lock_down=true;

		if(0 == dev->enable){
			printk(KERN_ERR "sprdfb: sprdfb_dispc_enable_overlay fail. (dev not enable)\n");
			goto ERROR_ENABLE_OVERLAY;
		}

		if(0 != swdispc_sync(dev)){
			printk(KERN_ERR "sprdfb: sprdfb_dispc_enable_overlay fail. (wait done fail)\n");
			goto ERROR_ENABLE_OVERLAY;
		}

		result = swdispc_overlay_img_configure(dev, info->data_type, &(info->rect), info->buffer, info->y_endian, info->uv_endian, info->rb_switch);
		if(0 != result){
			result=-1;
			printk(KERN_ERR "sprdfb: swdispc_overlay_img_configure fail.\n");
			goto ERROR_ENABLE_OVERLAY;
		}
		/*result = overlay_start(dev);*/
	}else{   /*disable*/
		/*result = overlay_close(dev);*/
	}
ERROR_ENABLE_OVERLAY:
	if(is_refresh_lock_down){
		up(&dev->refresh_lock);
	}

	pr_debug("sprdfb: [%s] return %d\n", __FUNCTION__, result);
	return result;
}


#ifdef CONFIG_FB_ESD_SUPPORT

static int32_t sprdfb_dispc_check_esd(struct sprdfb_device *dev)
{
	uint32_t ret = 0;
	bool	is_refresh_lock_down=false;

	pr_debug("sprdfb: [%s] \n", __FUNCTION__);

	if(SPRDFB_PANEL_IF_DBI == dev->panel_if_type){
		printk("sprdfb: [%s] leave (not support dbi mode now)!\n", __FUNCTION__);
		ret = -1;
		goto ERROR_CHECK_ESD;
	}
	down(&dev->refresh_lock);
	is_refresh_lock_down=true;
	if(0 == dev->enable){
		printk("sprdfb: [%s] leave (Invalid device status)!\n", __FUNCTION__);
		ret=-1;
		goto ERROR_CHECK_ESD;
	}

        if(0 == (dev->check_esd_time % 30)){
	   // printk("sprdfb: [%s] (%d, %d, %d)\n",__FUNCTION__, dev->check_esd_time, dev->panel_reset_time, dev->reset_dsi_time);
	}else{
	   // pr_debug("sprdfb: [%s] (%d, %d, %d)\n",__FUNCTION__, dev->check_esd_time, dev->panel_reset_time, dev->reset_dsi_time);
	}
ERROR_CHECK_ESD:
	if(is_refresh_lock_down){
		up(&dev->refresh_lock);
	}

	return ret;
}
#endif
int32_t sprdfb_is_refresh_done(struct sprdfb_device *dev)
{

	return (int32_t)swdispc_ctx.vsync_done;
}


struct display_ctrl sprdfb_swdispc_ctrl = {
	.name		= "swdispc",
	.early_init		= sprdfb_swdispc_early_init,
	.init		 	= sprdfb_swdispc_init,
	.uninit		= sprdfb_swdispc_uninit,
	.refresh		= sprdfb_swdispc_refresh,
	.logo_proc		= sprdfb_swdispc_logo_proc,
	.shutdown		= sprdfb_swdispc_shutdown,
	.suspend		= sprdfb_swdispc_suspend,
	.resume		= sprdfb_swdispc_resume,
#ifdef CONFIG_FB_VSYNC_SUPPORT
	.wait_for_vsync = spdfb_swdispc_wait_for_vsync,
#endif
#ifdef CONFIG_FB_ESD_SUPPORT
	.ESD_check	= sprdfb_dispc_check_esd,
#endif
#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	.enable_overlay = sprdfb_swdispc_enable_overlay,
	.display_overlay = sprdfb_swdispc_display_overlay,
#endif

	.is_refresh_done = sprdfb_is_refresh_done,

};


