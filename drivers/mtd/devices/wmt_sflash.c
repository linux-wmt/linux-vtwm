/*
 * Copyright (C) 2012 Tony Prisk <linux@prisktech.co.nz>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/io.h>
#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#include <linux/mtd/mtd.h>

/* controller only supports erase size of 64KB */
#define WMT_ERASESIZE			0x10000

/* Serial Flash controller register offsets */
#define SF_CHIP_SEL_0_CFG		0x000
#define SF_CHIP_SEL_1_CFG		0x008
#define SF_SPI_INTF_CFG			0x040
#define SF_SPI_RD_WR_CTR		0x050
#define SF_SPI_WR_EN_CTR		0x060
#define SF_SPI_ER_CTR			0x070
#define SF_SPI_ER_START_ADDR		0x074
#define SF_SPI_ERROR_STATUS		0x080
#define SF_SPI_MEM_0_SR_ACC		0x100
#define SF_SPI_MEM_1_SR_ACC		0x110
#define SF_SPI_PDWN_CTR_0		0x180
#define SF_SPI_PDWN_CTR_1		0x190
#define SF_SPI_PROG_CMD_CTR		0x200
#define SF_SPI_USER_CMD_VAL		0x210
#define SF_SPI_PROG_CMD_WBF		0x300	/* 64 bytes */
#define SF_SPI_PROG_CMD_RBF		0x380	/* 64 bytes */

/* SF_SPI_WR_EN_CTR bit fields */
#define SF_CS0_WR_EN			BIT(0)
#define SF_CS1_WR_EN			BIT(1)

/* SF_SPI_ER_CTR bit fields */
#define SF_SEC_ER_EN			BIT(31)

/* SF_SPI_ERROR_STATUS bit fields */
#define SF_ERR_TIMEOUT			BIT(31)
#define SF_ERR_WR_PROT			BIT(5)
#define SF_ERR_MEM_REGION		BIT(4)
#define SF_ERR_PWR_DWN_ACC		BIT(3)
#define SF_ERR_PCMD_OP			BIT(2)
#define SF_ERR_PCMD_ACC			BIT(1)
#define SF_ERR_MASLOCK			BIT(0)

/*
 * Serial Flash device manufacturers
 * Please keep sorted by manufacturers ID
 */
#define MFR_SPANSION		0x01
#define MFR_EON			0x1C
#define MFR_ATMEL		0x1F
#define MFR_NUMONYX		0x20
#define MFR_FUDAN		0xA1
#define MFR_SST			0xBF
#define MFR_MXIC		0xC2
#define MFR_WINBOND		0xEF

/*
 * SF Device Models
 * Please keep in the same order as the manufacturers table
 */

/* Spansion */
#define SPAN_FL016A		0x0214 /* 2 MB */
#define SPAN_FL064A		0x0216 /* 8 MB */

/* Eon */
#define EON_25P16		0x2015 /* 2 MB */
#define EON_25P64		0x2017 /* 8 MB */
#define EON_25F40		0x3113 /* 512 KB */
#define EON_25F16		0x3115 /* 2 MB */

/* Atmel */
#define AT_25DF041A		0x4401 /* 512KB */

/* Numonyx */
#define	NX_25P16		0x2015 /* 2 MB */
#define	NX_25P64		0x2017 /* 8 MB */

/* Fudan Microelectronics Group */
#define FM_25F04		0x3113 /* 512 KB */

/* SST */
#define SST_VF016B		0x2541 /* 2 MB */

/* MXIC */
#define	MX_L512			0x2010 /* 64 KB , 4KB*/
#define MX_L4005A		0x2013 /* 512 KB */
#define	MX_L1605D		0x2015 /* 2 MB */
#define	MX_L3205D		0x2016 /* 4 MB */
#define	MX_L6405D		0x2017 /* 8 MB */
#define	MX_L1635D		0x2415 /* 2 MB */
#define	MX_L3235D		0x5E16 /* 4 MB */
#define	MX_L12805D		0x2018 /* 16 MB */

/* WinBond */
#define WB_W25X40BV		0x3013	/* 512 KB */
#define WB_X16A			0x3015	/* 2 MB */
#define WB_X32			0x3016	/* 4 MB */
#define WB_X64			0x3017	/* 8 MB */


#define SF_ID(mfr, mdl)		((mfr << 16) | mdl)

#define FLASH_UNKNOWN		0x00ffffff

struct wmt_flash_id {
	u32	id;
	u32	size;		/* Size in KB */
};

static struct wmt_flash_id flash_ids[] = {
	{ SF_ID(MFR_SPANSION,	SPAN_FL016A),	2048 },
	{ SF_ID(MFR_SPANSION,	SPAN_FL064A),	8192 },
	{ SF_ID(MFR_EON,	EON_25P16),	2048 },
	{ SF_ID(MFR_EON,	EON_25P64),	8192 },
	{ SF_ID(MFR_EON,	EON_25F40),	512 },
	{ SF_ID(MFR_EON,	EON_25F16),	2048 },
	{ SF_ID(MFR_ATMEL,	AT_25DF041A),	512 },
	{ SF_ID(MFR_NUMONYX,	NX_25P16),	2048 },
	{ SF_ID(MFR_NUMONYX,	NX_25P64),	8192 },
	{ SF_ID(MFR_FUDAN,	FM_25F04),	512 },
	{ SF_ID(MFR_SST,	SST_VF016B),	2048 },
	{ SF_ID(MFR_MXIC,	MX_L512),	64 },
	{ SF_ID(MFR_MXIC,	MX_L4005A),	512 },
	{ SF_ID(MFR_MXIC,	MX_L1605D),	2048 },
	{ SF_ID(MFR_MXIC,	MX_L3205D),	4192 },
	{ SF_ID(MFR_MXIC,	MX_L6405D),	8192 },
	{ SF_ID(MFR_MXIC,	MX_L1635D),	2048 },
	{ SF_ID(MFR_MXIC,	MX_L3235D),	4192 },
	{ SF_ID(MFR_MXIC,	MX_L12805D),	16384 },
	{ SF_ID(MFR_WINBOND,	WB_W25X40BV),	512 },
	{ SF_ID(MFR_WINBOND,	WB_X16A),	2048 },
	{ SF_ID(MFR_WINBOND,	WB_X32),	4096 },
	{ SF_ID(MFR_WINBOND,	WB_X64),	8192 },
	{ 0, 0 },
};

struct wmt_sf_chip {
	u32	id;
	u32	size;
	u32	addr_phys;
	u32	ccr;
};

struct wmt_sf_data {
	struct mtd_info		*sf_mtd;
	struct clk		*sf_clk;
	struct device		*dev;

	struct wmt_sf_chip	chip[2];

	void __iomem		*base;		/* register virt base */

	void __iomem		*sf_base_virt;	/* mem-mapped sf virt base */
	u32			sf_base_phys;	/* mem-mapped sf phys base */
	u32			sf_total_size;
};

static u32 sf_get_chip_size(struct device *dev, u32 id)
{
	int i;
	for (i = 0; flash_ids[i].id != 0; i++)
		if (flash_ids[i].id == id)
			return flash_ids[i].size * 1024;

	dev_err(dev, "Unknown flash id (%08x)\n", id);
	return 0;
}

static void sf_calc_ccr(struct wmt_sf_chip *chip)
{
	unsigned int cnt = 0, size;

	size = chip->size;
	while (size) {
		size >>= 1;
		cnt++;
	}
	cnt -= 16;
	cnt = cnt << 8;
	chip->ccr = (chip->addr_phys | cnt);
}

static int wmt_sf_init_hw(struct wmt_sf_data *info)
{
	u32 phys_addr;

	phys_addr = 0xFFFFFFFF;
	writel(0x00000011, info->base + SF_SPI_RD_WR_CTR);
	writel(0xFF800800, info->base + SF_CHIP_SEL_0_CFG);
	writel(0x00030000, info->base + SF_SPI_INTF_CFG);

	info->chip[0].id = FLASH_UNKNOWN;
	info->chip[1].id = FLASH_UNKNOWN;

	/* Read serial flash ID */
	writel(0x11, info->base + SF_SPI_RD_WR_CTR);
	info->chip[0].id = readl(info->base + SF_SPI_MEM_0_SR_ACC);
	writel(0x01, info->base + SF_SPI_RD_WR_CTR);

	writel(0x11, info->base + SF_SPI_RD_WR_CTR);
	info->chip[1].id = readl(info->base + SF_SPI_MEM_1_SR_ACC);
	writel(0x01, info->base + SF_SPI_RD_WR_CTR);

	info->chip[0].size = sf_get_chip_size(info->dev, info->chip[0].id);
	if (info->chip[0].size == 0)
		return -1;

	info->chip[0].addr_phys = phys_addr - info->chip[0].size + 1;
	if (info->chip[0].addr_phys & 0xffff) {
		dev_err(info->dev, "Chip 0 start address must align to 64KB\n");
		return -1;
	}
	info->sf_base_phys = info->chip[0].addr_phys;
	info->sf_total_size = info->chip[0].size;
	pr_info("SFC: Chip 0 @ %08x (size: %d)\n", info->chip[0].addr_phys,
							info->chip[0].size);

	sf_calc_ccr(&info->chip[0]);
	writel(info->chip[0].ccr, info->base + SF_CHIP_SEL_0_CFG);

	if (info->chip[1].id != FLASH_UNKNOWN) {
		info->chip[1].size = sf_get_chip_size(info->dev,
						      info->chip[1].id);
		info->chip[1].addr_phys = info->chip[0].addr_phys -
							info->chip[1].size;
		if (info->chip[1].addr_phys & 0xffff) {
			dev_err(info->dev, "Chip 1 start address must align to 64KB\n");
			info->chip[1].id = FLASH_UNKNOWN;
			return 0;
		}
		info->sf_base_phys = info->chip[1].addr_phys;
		info->sf_total_size += info->chip[1].size;
		pr_info("SFC: Chip 1 @ %08x (size: %d)\n",
				info->chip[1].addr_phys, info->chip[1].size);

		sf_calc_ccr(&info->chip[1]);
		writel(info->chip[1].ccr, info->base + SF_CHIP_SEL_1_CFG);
	}

	return 0;
}

static int sf_check_error(struct device *dev, u32 code)
{
	if (code & SF_ERR_TIMEOUT) {
		dev_err(dev, "Serial flash timeout\n");
		return -ETIMEDOUT;
	}

	if (code & SF_ERR_WR_PROT) {
		dev_err(dev, "Serial flash write-protected\n");
		return -EIO;
	}

	if (code & SF_ERR_MEM_REGION) {
		dev_err(dev, "Serial flash memory region error\n");
		return -EIO;
	}

	if (code & SF_ERR_PWR_DWN_ACC) {
		dev_err(dev, "Serial flash power down access error\n");
		return -EIO;
	}

	if (code & SF_ERR_PCMD_OP)	{
		dev_err(dev, "Serial flash program CMD OP error\n");
		return -EIO;
	}

	if (code & SF_ERR_PCMD_ACC) {
		dev_err(dev, "Serial flash program CMD OP access error\n");
		return -EIO;
	}

	if (code & SF_ERR_MASLOCK) {
		dev_err(dev, "Serial flash master lock error\n");
		return -EIO;
	}

	/* no error */
	return 0;
}

static int sf_spi_read_status(struct wmt_sf_data *info, int chip)
{
	u32 timeout = 0x30000000;
	u32 temp;
	int err;

	do {
		if (chip == 0)
			temp = readl_relaxed(info->base + SF_SPI_MEM_0_SR_ACC);
		else
			temp = readl_relaxed(info->base + SF_SPI_MEM_1_SR_ACC);

		if ((temp & 0x1) == 0x0)
			break;

		err = sf_check_error(info->dev,
				     readl(info->base + SF_SPI_ERROR_STATUS));
		if (err) {
			writel(0x3f, info->base + SF_SPI_ERROR_STATUS);
			return err;
		}
		timeout--;
	} while (timeout);

	if (timeout == 0) {
		dev_err(info->dev, "spi request timed-out\n");
		return -ETIMEDOUT;
	}

	return 0;
}

#ifdef CONFIG_MTD_WMT_SFLASH_READONLY
static int sf_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	return -EINVAL;
}

static int sf_write(struct mtd_info *mtd, loff_t to, size_t len,
				size_t *retlen, const u_char *buf)
{
	*retlen = 0;
	return -EINVAL;
}
#else
static int sf_sector_erase(struct wmt_sf_data *info, u32 addr)
{
	int chip;
	u32 val;

	if ((info->sf_base_phys + addr) < info->chip[0].addr_phys) {
		chip = 0;
		writel(SF_CS0_WR_EN, info->base + SF_SPI_WR_EN_CTR);
	} else {
		chip = 1;
		writel(SF_CS1_WR_EN, info->base + SF_SPI_WR_EN_CTR);
	}

	addr &= ~(info->sf_mtd->erasesize - 1);
	writel(addr, info->base + SF_SPI_ER_START_ADDR);

	writel(SF_SEC_ER_EN, info->base + SF_SPI_ER_CTR);

	val = sf_spi_read_status(info, chip);

	writel(0, info->base + SF_SPI_WR_EN_CTR);
	return val;
}

static int sf_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct wmt_sf_data *info = mtd->priv;
	int ret;

	ret = clk_enable(info->sf_clk);
	if (ret)
		return ret;

	ret = sf_sector_erase(info, (u32)instr->addr);
	if (ret) {
		clk_disable(info->sf_clk);
		return ret;
	}

	instr->state = MTD_ERASE_DONE;
	mtd_erase_callback(instr);

	clk_disable(info->sf_clk);
	return 0;
}

static int sf_sector_write(struct wmt_sf_data *info, loff_t to, size_t len,
			   const u_char *buf)
{
	int ret;
	int data_size;
	u32 count;
	u32 addr_to = (u32)(info->sf_base_virt) + to;

	ret = clk_enable(info->sf_clk);
	if (ret)
		return ret;

	if (sf_spi_read_status(info, 0))
		return -EBUSY;
	if (sf_spi_read_status(info, 1))
		return -EBUSY;

	writel(SF_CS0_WR_EN | SF_CS1_WR_EN, info->base + SF_SPI_WR_EN_CTR);

	count = 0;
	while (len) {
		data_size = (len >= 4) ? 4 : 1;
		memcpy_toio(((u_char *)(addr_to + count)), buf + count,
								data_size);
		count += data_size;
		len -= data_size;

		if (len) {
			data_size = (len >= 4) ? 4 : 1;
			memcpy_toio(((u_char *)(addr_to + count)), buf + count,
								data_size);
			count += data_size;
			len -= data_size;
		}

		ret = sf_spi_read_status(info, 0);
		if (ret) {
			clk_disable(info->sf_clk);
			return ret;
		}
	}

	writel(0, info->base + SF_SPI_WR_EN_CTR);

	clk_disable(info->sf_clk);

	return count;
}

static int sf_write(struct mtd_info *mtd, loff_t to, size_t len,
				size_t *retlen, const u_char *buf)
{
	struct wmt_sf_data *info = mtd->priv;

	*retlen = sf_sector_write(info, to, len, buf);

	return 0;
}
#endif

static int sf_read(struct mtd_info *mtd, loff_t from, size_t len,
			 size_t *retlen, u_char *buf)
{
	int ret;
	struct wmt_sf_data *info = mtd->priv;

	ret = clk_enable(info->sf_clk);
	if (ret)
		return ret;

	if (sf_spi_read_status(info, 0))
		return -EBUSY;
	if (sf_spi_read_status(info, 1))
		return -EBUSY;

	if (from + len > mtd->size) {
		dev_err(info->dev, "Request out of bounds (from=%llu, len=%d)\n",
								from, len);
		return -EINVAL;
	}

	memcpy_fromio(buf, info->sf_base_virt + from, len);
	*retlen = len;

	clk_disable(info->sf_clk);
	return 0;
}

static int mtdsf_init_device(struct device *dev, struct mtd_info *mtd,
					unsigned long size, char *name)
{
	mtd->name = name;
	mtd->type = MTD_NORFLASH;
	mtd->flags = MTD_CAP_NORFLASH;
	mtd->size = size;
	mtd->erasesize = WMT_ERASESIZE;
	mtd->owner = THIS_MODULE;
	mtd->_erase = sf_erase;
	mtd->_read = sf_read;
	mtd->_write = sf_write;
	mtd->writesize = 1;

	if (mtd_device_register(mtd, NULL, 0)) {
		dev_err(dev, "Erroring adding MTD device\n");
		return -EIO;
	}

	return 0;
}

static int wmt_sf_probe(struct platform_device *pdev)
{
	struct wmt_sf_data	*info;
	struct device_node	*np = pdev->dev.of_node;
	int err;

	if (!np) {
		dev_err(&pdev->dev, "Invalid devicetree node\n");
		return -EINVAL;
	}

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "Failed to get memory for SF info\n");
		return -ENOMEM;
	}

	info->dev = &pdev->dev;

	info->base = of_iomap(np, 0);
	if (!info->base) {
		dev_err(&pdev->dev, "Failed to map register memory\n");
		return -ENOMEM;
	}

	info->sf_clk = of_clk_get(np, 0);
	if (!info->sf_clk) {
		dev_err(&pdev->dev, "Failed to get clock from device tree\n");
		return -EINVAL;
	}

	err = clk_prepare_enable(info->sf_clk);
	if (err)
		return err;

	err = wmt_sf_init_hw(info);

	clk_disable(info->sf_clk);

	if (err) {
		dev_err(&pdev->dev, "Failed to initialize SF hardware\n");
		return -EIO;
	}

	info->sf_base_virt = devm_ioremap(&pdev->dev, info->sf_base_phys,
					  info->sf_total_size);
	if (!info->sf_base_virt) {
		dev_err(&pdev->dev, "Failed to map serial flash memory\n");
		return -ENOMEM;
	}

	info->sf_mtd = devm_kzalloc(&pdev->dev, sizeof(struct mtd_info),
								GFP_KERNEL);
	if (!info->sf_mtd) {
		dev_err(&pdev->dev, "Failed to allocate SFMTD memory\n");
		return -ENOMEM;
	}

	err = mtdsf_init_device(info->dev, info->sf_mtd, info->sf_total_size,
						"Wondermedia SF Device");
	if (err)
		return err;

	info->sf_mtd->priv = info;
	dev_set_drvdata(&pdev->dev, info);

	pr_info("Wondermedia Serial Flash Controller initialized\n");

	return 0;
}

static int wmt_sf_remove(struct platform_device *pdev)
{
	struct wmt_sf_data *info = dev_get_drvdata(&pdev->dev);

	mtd_device_unregister(info->sf_mtd);

	return 0;
}

static const struct of_device_id wmt_dt_ids[] = {
	{ .compatible = "wm,wm8505-sf", },
	{}
};

static struct platform_driver wmt_sf_driver = {
	.probe		= wmt_sf_probe,
	.remove		= wmt_sf_remove,
	.driver		= {
		.name	= "wmt-sf",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(wmt_dt_ids),
	},
};

module_platform_driver(wmt_sf_driver);

MODULE_AUTHOR("Tony Prisk <linux@prisktech.co.nz>");
MODULE_DESCRIPTION("Wondermedia SoC Serial Flash driver");
MODULE_LICENSE("GPL v2");

