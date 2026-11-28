/*
 * DRM driver for Sitronix ST7789 panels (170x320)
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/spi/spi.h>

#include <drm/clients/drm_client_setup.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_fbdev_dma.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_managed.h>
#include <drm/drm_mipi_dbi.h>
#include <drm/drm_modeset_helper.h>
#include <drm/drm_print.h>
#include <video/mipi_display.h>

/* MADCTL bits */
#define ST7789_MADCTL_MY	BIT(7)
#define ST7789_MADCTL_MX	BIT(6)
#define ST7789_MADCTL_MV	BIT(5)
#define ST7789_MADCTL_ML	BIT(4)
#define ST7789_MADCTL_BGR	BIT(3)
#define ST7789_MADCTL_MH	BIT(2)

/* Смещения матрицы из Afruit_ST7789.cpp */
#define ST7789_170x320_XSTART	0x23
#define ST7789_170x320_YSTART	0x00

static void st7789_enable(struct drm_simple_display_pipe *pipe,
			  struct drm_crtc_state *crtc_state,
			  struct drm_plane_state *plane_state)
{
	struct mipi_dbi_dev *dbidev = drm_to_mipi_dbi_dev(pipe->crtc.dev);
	struct mipi_dbi *dbi = &dbidev->dbi;
	u8 addr_mode;
	int ret, idx;

	if (!drm_dev_enter(pipe->crtc.dev, &idx))
		return;

	dev_info(dbidev->drm.dev, "ST7789 - enable (RGB666 / 3-byte SPI mode)\n");

	ret = mipi_dbi_poweron_conditional_reset(dbidev);
	if (ret < 0)
		goto out_exit;
	if (ret == 1)
		goto out_enable;

	/* 1: Software reset */
	mipi_dbi_command(dbi, MIPI_DCS_SOFT_RESET);
	msleep(150);

	/* 2: Out of sleep mode */
	mipi_dbi_command(dbi, MIPI_DCS_EXIT_SLEEP_MODE);
	msleep(500);

	/* 3: Set pixel format - 18-bit (RGB666, 3 байта на пиксель по SPI) */
	mipi_dbi_command(dbi, MIPI_DCS_SET_PIXEL_FORMAT, 0x66);
	msleep(10);

	/* 4: Memory access control */
	mipi_dbi_command(dbi, MIPI_DCS_SET_ADDRESS_MODE, 0x08);

	/* 5: Column addr set (XSTART = 0, XEND = 170) */
	mipi_dbi_command(dbi, MIPI_DCS_SET_COLUMN_ADDRESS, 0x00, 0x00, 0x00, 0xaa);

	/* 6: Row addr set (YSTART = 0, YEND = 320) */
	mipi_dbi_command(dbi, MIPI_DCS_SET_PAGE_ADDRESS, 0x00, 0x00, 0x01, 0x3f);

	/* 7: Display inversion on */
	mipi_dbi_command(dbi, MIPI_DCS_ENTER_INVERT_MODE);
	msleep(10);

	/* 8: Normal display on */
	mipi_dbi_command(dbi, MIPI_DCS_ENTER_NORMAL_MODE);
	msleep(10);

	/* 9: Main screen turn on */
	mipi_dbi_command(dbi, MIPI_DCS_SET_DISPLAY_ON);
	msleep(500);

out_enable:
	switch (dbidev->rotation) {
	default:
		addr_mode = ST7789_MADCTL_MX | ST7789_MADCTL_MY;
		dbidev->left_offset = ST7789_170x320_XSTART;
		dbidev->top_offset = ST7789_170x320_YSTART;
		break;
	case 90:
		addr_mode = ST7789_MADCTL_MY | ST7789_MADCTL_MV;
		dbidev->left_offset = ST7789_170x320_YSTART;
		dbidev->top_offset = ST7789_170x320_XSTART;
		break;
	case 180:
		addr_mode = 0;
		dbidev->left_offset = ST7789_170x320_XSTART;
		dbidev->top_offset = ST7789_170x320_YSTART;
		break;
	case 270:
		addr_mode = ST7789_MADCTL_MX | ST7789_MADCTL_MV;
		dbidev->left_offset = ST7789_170x320_YSTART;
		dbidev->top_offset = ST7789_170x320_XSTART;
		break;
	}
	addr_mode |= ST7789_MADCTL_BGR;
	
	mipi_dbi_command(dbi, MIPI_DCS_SET_ADDRESS_MODE, addr_mode);

	mipi_dbi_enable_flush(dbidev, crtc_state, plane_state);
out_exit:
	drm_dev_exit(idx);
}

static const struct drm_simple_display_pipe_funcs st7789_pipe_funcs = {
	DRM_MIPI_DBI_SIMPLE_DISPLAY_PIPE_FUNCS(st7789_enable),
};

static const struct drm_display_mode st7789_mode = {
	DRM_SIMPLE_MODE(170, 320, 23, 43),
};

static const uint32_t st7789_formats[] = {
	DRM_FORMAT_RGB888,
	DRM_FORMAT_XRGB8888,
};

DEFINE_DRM_GEM_DMA_FOPS(st7789_fops);

static const struct drm_driver st7789_driver = {
	.driver_features	= DRIVER_GEM | DRIVER_MODESET | DRIVER_ATOMIC,
	.fops			= &st7789_fops,
	DRM_GEM_DMA_DRIVER_OPS_VMAP,
	DRM_FBDEV_DMA_DRIVER_OPS,
	.debugfs_init		= mipi_dbi_debugfs_init,
	.name			= "st7789_rgb888",
	.desc			= "Sitronix ST7789 170x320 (RGB888)",
	.major			= 1,
	.minor			= 0,
};

static const struct of_device_id st7789_of_match[] = {
	{ .compatible = "sitronix,st7789v-170x320" },
	{ }
};
MODULE_DEVICE_TABLE(of, st7789_of_match);

static const struct spi_device_id st7789_id[] = {
	{ "st7789v-170x320", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, st7789_id);

static int st7789_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct mipi_dbi_dev *dbidev;
	struct drm_device *drm;
	struct mipi_dbi *dbi;
	struct gpio_desc *dc;
	u32 rotation = 0;
	int ret;

	dev_info(dev, "ST7789 Probe start (RGB888 mode)\n");

	dbidev = devm_drm_dev_alloc(dev, &st7789_driver, struct mipi_dbi_dev, drm);
	if (IS_ERR(dbidev))
		return PTR_ERR(dbidev);

	dbi = &dbidev->dbi;
	drm = &dbidev->drm;

	dbi->reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(dbi->reset))
		return dev_err_probe(dev, PTR_ERR(dbi->reset), "Failed to get GPIO 'reset'\n");

	dc = devm_gpiod_get_optional(dev, "dc", GPIOD_OUT_LOW);
	if (IS_ERR(dc))
		return dev_err_probe(dev, PTR_ERR(dc), "Failed to get GPIO 'dc'\n");

	dbidev->backlight = devm_of_find_backlight(dev);
	if (IS_ERR(dbidev->backlight))
		return PTR_ERR(dbidev->backlight);

	device_property_read_u32(dev, "rotation", &rotation);

	ret = mipi_dbi_spi_init(spi, dbi, dc);
	if (ret)
		return ret;

	dev_info(dev, "SPI from DT: %d Hz\n", spi->max_speed_hz);

	/*
	 * Инициализация с поддержкой RGB888 формата без конверсии в RGB565.
	 * tx_buf_size: 170 * 320 * 4 (макс 32 бита на пиксель)
	 */
	ret = mipi_dbi_dev_init_with_formats(dbidev, &st7789_pipe_funcs,
					     st7789_formats,
					     ARRAY_SIZE(st7789_formats),
					     &st7789_mode, rotation,
					     170 * 320 * 4);
	if (ret) {
		dev_err(dev, "Failed to initialize DBI device: %d\n", ret);
		return ret;
	}

	drm_mode_config_reset(drm);

	ret = drm_dev_register(drm, 0);
	if (ret) {
		dev_err(dev, "Failed to register DRM device: %d\n", ret);
		return ret;
	}

	spi_set_drvdata(spi, drm);

	drm_client_setup(drm, NULL);

	dev_info(dev, "ST7789 Probe successful\n");
	return 0;
}

static void st7789_remove(struct spi_device *spi)
{
	struct drm_device *drm = spi_get_drvdata(spi);

	dev_info(&spi->dev, "ST7789 - remove\n");
	drm_dev_unplug(drm);
	drm_atomic_helper_shutdown(drm);
}

static void st7789_shutdown(struct spi_device *spi)
{
	dev_info(&spi->dev, "ST7789 - shutdown\n");
	drm_atomic_helper_shutdown(spi_get_drvdata(spi));
}

static struct spi_driver st7789_spi_driver = {
	.driver = {
		.name = "st7789_170x320",
		.of_match_table = st7789_of_match,
	},
	.id_table = st7789_id,
	.probe = st7789_probe,
	.remove = st7789_remove,
	.shutdown = st7789_shutdown,
};
module_spi_driver(st7789_spi_driver);

MODULE_DESCRIPTION("Sitronix ST7789 170x320 DRM driver");
MODULE_AUTHOR("Sergey Horoshevskiy shaudioprojects@gmail.com");
MODULE_LICENSE("GPL");
