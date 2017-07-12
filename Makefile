obj-$(CONFIG_FB_SCX35) := sprdfb_spi_intf.o
obj-$(CONFIG_FB_SCX35) += sprdfb_main.o sprdfb_panel.o sprdfb_dispc.o sprdfb_mcu.o sprdfb_rgb.o sprdfb_mipi.o sprdfb_i2c.o sprdfb_spi.o sprdfb_dsi.o  sprdfb_chip_common.o sprdfb_chip_8830.o
obj-$(CONFIG_FB_SCX35) += dsi_1_10a/
obj-$(CONFIG_FB_SCX35) += lcd/

obj-$(CONFIG_FB_SCX30G) := sprdfb_spi_intf.o
obj-$(CONFIG_FB_SCX30G) += sprdfb_main.o sprdfb_panel.o sprdfb_dispc.o sprdfb_mcu.o sprdfb_rgb.o sprdfb_mipi.o sprdfb_i2c.o sprdfb_spi.o sprdfb_dsi.o  sprdfb_chip_common.o sprdfb_chip_8830.o
obj-$(CONFIG_FB_SCX30G) += dsi_1_21a/
obj-$(CONFIG_FB_SCX30G) += lcd/

obj-$(CONFIG_FB_SCX15) := sprdfb_spi_intf.o
obj-$(CONFIG_FB_SCX15) += sprdfb_main.o sprdfb_panel.o sprdfb_dispc.o sprdfb_mcu.o sprdfb_rgb.o sprdfb_i2c.o sprdfb_spi.o sprdfb_chip_common.o sprdfb_chip_7715.o
obj-$(CONFIG_FB_SCX15) += lcd/
obj-$(CONFIG_FB_DYNAMIC_FPS_SUPPORT) += sprdfb_notifier.o

# ========== SharkL/TSharkL configuration begin ===========
ifeq ($(CONFIG_FB_SCX35L), y)
obj-y += sprdfb_spi_intf.o
obj-y += sprdfb_main.o sprdfb_panel.o
obj-y += sprdfb_dsi.o sprdfb_mipi.o sprdfb_rgb.o sprdfb_mcu.o
obj-y += sprdfb_chip_common.o sprdfb_chip_9630.o
obj-$(CONFIG_I2C) += sprdfb_i2c.o
obj-$(CONFIG_SPI) += sprdfb_spi.o
ifeq ($(CONFIG_FB_SWDISPC), y)
obj-y += sprdfb_swdispc.o
else
obj-y += sprdfb_dispc.o
endif
endif
ifeq ($(CONFIG_SC_FPGA), y)
obj-$(CONFIG_FB_SCX35L) += sprdfb_gpio2spi.o
endif

obj-$(CONFIG_FB_SCX35L) += dsi_1_21a/
obj-$(CONFIG_FB_SCX35L) += lcd/
# ========== SharkL/TSharkL configuration end ============
# ========== whale configuration begin ===========
ifeq ($(CONFIG_FB_SC9001), y)
obj-y := sprdfb_spi_intf.o
obj-y += sprdfb_main.o sprdfb_panel.o sprdfb_dispc.o
obj-y += sprdfb_dsi.o sprdfb_mipi.o
obj-y += sprdfb_chip_common.o sprdfb_chip_9001.o
obj-$(CONFIG_I2C) += sprdfb_i2c.o
//obj-$(CONFIG_SPI) += sprdfb_spi.o
endif

ifeq ($(CONFIG_SC_FPGA), y)
obj-$(CONFIG_FB_SC9001) += sprdfb_gpio2spi.o
endif

obj-$(CONFIG_FB_SC9001) += dsi_1_21a/
obj-$(CONFIG_FB_SC9001) += lcd/
# ========== whale configuration end ============
# This file is used for creating file node
obj-y += sprdfb_attr.o

