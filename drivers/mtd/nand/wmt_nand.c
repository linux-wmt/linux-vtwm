/*
 * WonderMedia WM8xxx-series SoC NAND Flash driver
 *
 * Copyright (C) 2013 Tony Prisk <linux@prisktech.co.nz>
 *
 * Based on GPLv2 source:
 * Copyright (C) 2011 Darek Marcinkiewicz <reksio@newterm.pl>
 * Copyright (C) 2010 Alexey Charkov <alchark@gmail.com>
 * Copyright (C) 2008 WonderMedia Technologies, Inc.
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_mtd.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#define DRIVER_NAME	"wmt-nand"

#define DMA_BUFFER_SIZE			16384
#define NAND_DIR_READ			0x00
#define NAND_DIR_WRITE			0x01

#define REG_DATAPORT			0x00
#define REG_COMCTRL			0x04
#define REG_COMPORT0			0x08
#define REG_COMPORT1_2			0x0C
#define REG_COMPORT3_4			0x10
#define REG_COMPORT5_6			0x14
#define REG_COMPORT7			0x18
#define REG_COMPORT8_9			0x1C
#define REG_DMA_COUNTER			0x20
#define REG_SMC_ENABLE			0x24
#define REG_MISC_STAT_PORT		0x28
#define REG_HOST_STAT_CHANGE		0x2C
#define REG_SMC_DMA_COUNTER		0x30
#define REG_CALC_CTRL			0x34
#define REG_CALC_NUM			0x38
#define REG_CALC_NUM_QU			0x3C
#define REG_REMAINDER			0x40
#define REG_CHIP_ENABLE_CTRL		0x44
#define REG_NAND_TYPE_SEL		0x48
#define REG_REDUNT_ECC_STAT_MASK	0x4C
#define REG_READ_CYCLE_PULE_CTRL	0x50
#define REG_MISC_CTRL			0x54
#define REG_DUMMY_CTRL			0x58
#define REG_PAGESIZE_DIVIDER_SEL	0x5C
#define REG_RW_STROBE_TUNE		0x60
#define REG_BANK18_ECC_STAT_MASK	0x64
#define REG_ODD_BANK_PARITY_STAT	0x68
#define REG_EVEN_BANK_PARITY_STAT	0x6C
#define REG_REDUNT_AREA_PARITY_STAT	0x70
#define REG_IDLE_STAT			0x74
#define REG_PHYS_ADDR			0x78
#define REG_REDUNT_ECC_STAT		0x7C
#define REG_BANK18_ECC_STAT		0x80
#define REG_TIMER_COUNTER_CONFIG	0x84
#define REG_NANDFLASH_BOOT		0x88
#define REG_ECC_BCH_CTRL		0x8C
#define REG_ECC_BCH_INT_MASK		0x90
#define REG_ECC_BCH_INT_STAT1		0x94
#define REG_ECC_BCH_INT_STAT2		0x98
#define REG_ECC_BCH_ERR_POS1		0x9C
#define REG_ECC_BCH_ERR_POS2		0xA0
#define REG_ECC_BCH_ERR_POS3		0xA4
#define REG_ECC_BCH_ERR_POS4		0xA8
#define REG_ECC_BCH_ERR_POS5		0xAC
#define REG_ECC_BCH_ERR_POS6		0xB0
#define REG_ECC_BCH_ERR_POS7		0xB4
#define REG_ECC_BCH_ERR_POS8		0xB8

#define REG_NFC_DMA_GCR			0x100
#define REG_NFC_DMA_IER			0x104
#define REG_NFC_DMA_ISR			0x108
#define REG_NFC_DMA_DESPR		0x10C
#define REG_NFC_DMA_RBR			0x110
#define REG_NFC_DMA_DAR			0x114
#define REG_NFC_DMA_BAR			0x118
#define REG_NFC_DMA_CPR			0x11C
#define REG_NFC_DMA_CCR			0x120

#define REG_ECC_FIFO_0			0x1C0
#define REG_ECC_FIFO_1			0x1C4
#define REG_ECC_FIFO_2			0x1C8
#define REG_ECC_FIFO_3			0x1CC
#define REG_ECC_FIFO_4			0x1D0
#define REG_ECC_FIFO_5			0x1D4
#define REG_ECC_FIFO_6			0x1D8
#define REG_ECC_FIFO_7			0x1DC
#define REG_ECC_FIFO_8			0x1E0
#define REG_ECC_FIFO_9			0x1E4
#define REG_ECC_FIFO_A			0x1E8
#define REG_ECC_FIFO_B			0x1EC
#define REG_ECC_FIFO_C			0x1F0
#define REG_ECC_FIFO_D			0x1F4
#define REG_ECC_FIFO_E			0x1F8
#define REG_ECC_FIFO_F			0x1FC

/* 0x04 */
#define COMCTRL_TRIGGER_CMD		BIT(0)
#define COMCTRL_MULT_COMMANDS		BIT(4)
#define COMCTRL_CYCLES_DMA		0
#define COMCTRL_CYCLES_NONE		0
#define COMCTRL_CYCLES_SINGLE		BIT(5)
#define COMCTRL_NFC_2_NAND		0
#define COMCTRL_NAND_2_NFC		BIT(6)
#define COMCTRL_HAS_DATA		0
#define COMCTRL_NO_DATA			BIT(7)
#define COMCTRL_OLD_CMD			BIT(10)

/* 0x28 */
#define MSP_READY			BIT(0)
#define MSP_TRANSFER_ACTIVE		BIT(1)
#define MSP_CMD_READY			BIT(2)

/* 0x2C */
#define HSC_B2R				BIT(3)

/* 0x48 */
#define TYPESEL_PAGE_512		0
#define TYPESEL_PAGE_2K			1
#define TYPESEL_PAGE_4K			2
#define TYPESEL_PAGE_8K			3
#define TYPESEL_OLDDATA_EN		BIT(2)
#define TYPESEL_WIDTH_8			0
#define TYPESEL_WIDTH_16		BIT(3)
#define TYPESEL_WP_DIS			BIT(4)
#define TYPESEL_DIRECT_MAP		BIT(5)
#define TYPESEL_CHECK_ALLFF		BIT(6)

/* 0x4C */
#define RESM_MASKABLE_INT_DIS		BIT(6)
#define RESM_B2R_DIS			BIT(3)
#define RESM_UNCORRECTABLE_ERR_INT_DIS	BIT(2)
#define RESM_1BIT_ERR_INT_DIS		BIT(1)
#define RESM_REDUNTANT_ERR_INT_DIS	BIT(0)

#define RESM_MASK			(RESM_MASKABLE_INT_DIS | \
					RESM_UNCORRECTABLE_ERR_INT_DIS | \
					RESM_REDUNTANT_ERR_INT_DIS)

/* 0x50 */
#define PULE_DIVISOR_MASK		0xffff0000;

/* 0x54 */
#define MISCCTRL_SOFTWARE_ECC		BIT(2)

/* 0x5C */
#define PAGE_BLOCK_DIVISOR_MASK		0xE0
#define PAGE_BLOCK_DIVISOR(x)		((x) << 5)

/* 0x8C */
#define EBC_ECC_TYPE_MASK		0xFFFFFFF0
#define EBC_ECC_1BIT			0
#define EBC_ECC_4BIT			1
#define EBC_ECC_8BIT			2
#define EBC_ECC_12BIT			3
#define EBC_ECC_16BIT			4
#define EBC_ECC_24BITPER1K		5
#define EBC_ECC_40BITPER1K		6
#define EBC_ECC_44BITPER1K		7
#define EBC_ECC_44BIT			8
#define EBC_READ_RESUME			BIT(8)

/* 0x90 */
#define EBIM_INT_EN			(BIT(8) | BIT(0))

/* 0x94 */
#define EBIS1_ERROR			BIT(0)
#define EBIS1_CORRECTION_DONE		BIT(8)

/* 0x98 */
#define EBIS2_ERROR_OOB			BIT(11)

/* 0x100 */
#define DMA_GCR_DMA_EN			BIT(0)
#define DMA_GCR_SOFTRESET		BIT(8)

/* 0x108 */
#define DMA_IER_INT_STS			BIT(0)

/* 0x120 */
#define DMA_CCR_EVTCODE			0x0f
#define DMA_CCR_EVT_NO_STATUS		0x00
#define DMA_CCR_EVT_FF_UNDERRUN		0x01
#define DMA_CCR_EVT_FF_OVERRUN		0x02
#define DMA_CCR_EVT_DESP_READ		0x03
#define DMA_CCR_EVT_DATA_RW		0x04
#define DMA_CCR_EVT_EARLY_END		0x05
#define DMA_CCR_EVT_SUCCESS		0x0f

#define DMA_CCR_RUN			BIT(7)
#define DMA_CCR_IF_TO_PERIPHERAL	0
#define DMA_CCR_PERIPHERAL_TO_IF	BIT(22)

static struct nand_ecclayout vt8500_oobinfo = {
	.eccbytes = 7,
	.eccpos = {24, 25, 26, 27, 28, 29, 30},
	.oobavail = 24,
	.oobfree = { {0, 24} }
};

struct nand_dma_desc {
	u32 req_count:16;
	u32 i:1;
	u32 r1:13;
	u32 format:1;
	u32 end:1;
	u32 addr:32;
	u32 branch_addr:32;
	u32 r2:32;
};

struct nand_priv {
	struct mtd_info mtd;
	struct nand_chip nand;

	struct device *dev;
	void __iomem *reg_base;
	struct clk *clk;

	dma_addr_t dmaaddr;
	unsigned char *dmabuf;

	dma_addr_t dma_d_addr;
	struct nand_dma_desc *dma_desc;

	int dataptr;

	int page;

	int nand_irq;
	int dma_irq;

	unsigned long dma_status;

	struct completion nand_complete;
	struct completion dma_complete;
};

static uint8_t nand_bbt_pattern[] = { 'B', 'b', 't', '0' };
static uint8_t nand_mirror_pattern[] = { '1', 't', 'b', 'B' };

static struct nand_bbt_descr nand_bbt_main_descr_2048 = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs =	4,
	.len = 4,
	.veroffs = 0,
	.maxblocks = 4,
	.pattern = nand_bbt_pattern,
};

static struct nand_bbt_descr nand_bbt_mirror_descr_2048 = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs =	4,
	.len = 4,
	.veroffs = 0,
	.maxblocks = 4,
	.pattern = nand_mirror_pattern,
};

struct nand_priv *wmt_mtd_to_priv(struct mtd_info *mtd)
{
	return container_of(mtd, struct nand_priv, mtd);
}

int wmt_nand_get_bit(struct nand_priv *priv, int address, unsigned bit)
{
	return readl(priv->nand.IO_ADDR_R + address) & bit;
}

void wmt_nand_set_bit(struct nand_priv *priv, int address, unsigned val)
{
	unsigned long t = readl(priv->nand.IO_ADDR_R + address);
	t |= val;
	writel(t, priv->nand.IO_ADDR_R + address);
}

void wmt_nand_clear_bit(struct nand_priv *priv, int address, unsigned bit)
{
	unsigned long t = readl(priv->nand.IO_ADDR_R + address);
	t &= ~bit;
	writel(t, priv->nand.IO_ADDR_R + address);
}

#define NAND_MAX_CLOCK_SPEED	50000000
static int wmt_nand_set_clock(struct nand_priv *priv)
{
	unsigned long ptRP = 12000;
	unsigned long ptRC = 25000;
	unsigned long ptWP = 12000;
	unsigned long ptWC = 25000;
	unsigned long reg;
	unsigned long pule_reg;
	unsigned long min_val;

	min_val = min(min(ptRP, ptRC), min(ptWP, ptWC));

	pule_reg = readl(priv->nand.IO_ADDR_R + REG_READ_CYCLE_PULE_CTRL);
	pule_reg &= PULE_DIVISOR_MASK;

	reg = ptRP / min_val;
	reg <<= 4;

	reg |= ptRC / min_val;
	reg <<= 4;

	reg |= ptWP / min_val;
	reg <<= 4;

	reg |= ptWC / min_val;

	clk_set_rate(priv->clk, NAND_MAX_CLOCK_SPEED);

	writel(pule_reg | reg, priv->nand.IO_ADDR_R + REG_READ_CYCLE_PULE_CTRL);

	return 0;
}

void __iomem *wmt_nand_addr_cycle_to_reg(struct nand_priv *priv, int cycle)
{
	u8 *addr_reg = priv->nand.IO_ADDR_R + REG_COMPORT1_2;
	return addr_reg + 4 * (cycle / 2) + cycle % 2;
}

static int wmt_nand_set_addr(struct nand_priv *priv, int column, int page_addr)
{
	struct nand_chip *chip = &priv->nand;
	int addr_cycle = 0;
	u8 *addr_reg;

	if (column != -1) {
		addr_reg = wmt_nand_addr_cycle_to_reg(priv, addr_cycle++);
		writeb(column, addr_reg);
		column >>= 8;

		addr_reg = wmt_nand_addr_cycle_to_reg(priv, addr_cycle++);
		writeb(column, addr_reg);
	}

	if (page_addr != -1) {
		addr_reg = wmt_nand_addr_cycle_to_reg(priv, addr_cycle++);
		writeb(page_addr, addr_reg);
		page_addr >>= 8;

		addr_reg = wmt_nand_addr_cycle_to_reg(priv, addr_cycle++);
		writeb(page_addr, addr_reg);

		if (chip->chip_shift - chip->page_shift > 16) {
			page_addr >>= 8;
			addr_reg = wmt_nand_addr_cycle_to_reg(priv, addr_cycle++);
			writeb(page_addr, addr_reg);
		}
	}

	return addr_cycle;
}

static void wmt_nand_clear_busy2ready(struct nand_priv *priv)
{
	wmt_nand_set_bit(priv, REG_HOST_STAT_CHANGE, HSC_B2R);
}

static int wmt_nand_get_busy2ready(struct nand_priv *priv)
{
	return wmt_nand_get_bit(priv, REG_HOST_STAT_CHANGE, HSC_B2R);
}

static int wmt_nand_wait_cmd_ready(struct nand_priv *priv)
{
	int loop_guard = (1 << 20);

	while (--loop_guard && wmt_nand_get_bit(priv, REG_MISC_STAT_PORT, MSP_CMD_READY))
		cpu_relax();

	BUG_ON(!loop_guard);

	return 0;
}

static int wmt_nand_wait_transfer_ready(struct nand_priv *priv)
{
	int loop_guard = (1 << 28);

	while (--loop_guard &&
		wmt_nand_get_bit(priv, REG_MISC_STAT_PORT, MSP_TRANSFER_ACTIVE))
		cpu_relax();

	BUG_ON(!loop_guard);

	return 0;
}

static int wmt_nand_device_ready(struct mtd_info *mtd)
{
	struct nand_priv *priv = wmt_mtd_to_priv(mtd);
	return wmt_nand_get_bit(priv, REG_MISC_STAT_PORT, MSP_READY);
}

static int wmt_nand_read_page(struct mtd_info *mtd, struct nand_chip *chip,
			       uint8_t *buf, int oob_required, int page)
{
	struct nand_priv *priv = wmt_mtd_to_priv(mtd);

	chip->read_buf(mtd, buf, mtd->writesize);
	memcpy_fromio(chip->oob_poi, priv->nand.IO_ADDR_R + REG_ECC_FIFO_0,
		      min(64u, mtd->oobsize));

	return 0;
}

static void wmt_nand_setup_dma_transfer(struct nand_priv *priv, int direction)
{
	unsigned long tmp;

	writew(priv->mtd.writesize - 1, priv->nand.IO_ADDR_R + REG_DMA_COUNTER);

	if (readl(priv->nand.IO_ADDR_R + REG_NFC_DMA_ISR) & DMA_IER_INT_STS) {
		int loop_guard = 1 << 20;

		writel(DMA_IER_INT_STS, priv->nand.IO_ADDR_R + REG_NFC_DMA_ISR);

		while (--loop_guard &&
			(readl(priv->nand.IO_ADDR_R + REG_NFC_DMA_ISR) &
			DMA_IER_INT_STS))
			cpu_relax();

		if (!loop_guard) {
			dev_err(priv->dev,
				"PDMA interrupt status can't be cleared");
			dev_err(priv->dev, "REG_NFC_DMA_ISR = 0x%8.8x\n",
				(unsigned int)readl(priv->nand.IO_ADDR_R +
						    REG_NFC_DMA_ISR));
			BUG();
		}
	}

	writel(DMA_GCR_SOFTRESET, priv->nand.IO_ADDR_R + REG_NFC_DMA_GCR);
	writel(DMA_GCR_DMA_EN, priv->nand.IO_ADDR_R + REG_NFC_DMA_GCR);
	/* check if we really succeeded */
	BUG_ON((readl(priv->nand.IO_ADDR_R + REG_NFC_DMA_GCR) & DMA_GCR_DMA_EN)
	       == 0);

	memset(priv->dma_desc, 0, sizeof(*priv->dma_desc));
	priv->dma_desc->req_count = priv->mtd.writesize;
	priv->dma_desc->format = 1;
	priv->dma_desc->i = 1;
	priv->dma_desc->addr = (u32) priv->dmaaddr;
	priv->dma_desc->end = 1;

	writel((u32) priv->dma_d_addr, priv->nand.IO_ADDR_R + REG_NFC_DMA_DESPR);

	/* set direction */
	tmp = readl(priv->nand.IO_ADDR_R + REG_NFC_DMA_CCR);
	if (direction == NAND_DIR_READ)
		tmp |= DMA_CCR_PERIPHERAL_TO_IF;
	else
		tmp &= ~DMA_CCR_IF_TO_PERIPHERAL;
	writel(tmp, priv->nand.IO_ADDR_R + REG_NFC_DMA_CCR);

	writel(1, priv->nand.IO_ADDR_R + REG_NFC_DMA_IER);

	tmp = readl(priv->nand.IO_ADDR_R + REG_NFC_DMA_CCR);
	tmp |= DMA_CCR_RUN;
	writel(tmp, priv->nand.IO_ADDR_R + REG_NFC_DMA_CCR);
}

static void wmt_nand_setup_command(struct nand_priv *priv, int flag,
				 int command_bytes)
{
	u8 byte = 0;
	byte |= flag;
	byte |= command_bytes << 1;
	writeb(byte, priv->nand.IO_ADDR_R + REG_COMCTRL);
}

static void wmt_nand_trigger_command(struct nand_priv *priv, int flag,
				   int command_bytes)
{
	flag |= COMCTRL_TRIGGER_CMD;
	wmt_nand_setup_command(priv, flag, command_bytes);
}

static int wmt_nand_wait_dma(struct nand_priv *priv)
{
	if (!wait_for_completion_interruptible_timeout
	    (&priv->dma_complete, msecs_to_jiffies(1000))) {
		dev_err(priv->dev, "Waiting for dma interrupt failed!\n");
		return -1;
	}

	if (priv->dma_status == DMA_CCR_EVT_FF_UNDERRUN)
		dev_err(priv->dev, "PDMA Buffer under run!\n");

	if (priv->dma_status == DMA_CCR_EVT_FF_OVERRUN)
		dev_err(priv->dev, "PDMA Buffer over run!\n");

	if (priv->dma_status == DMA_CCR_EVT_DESP_READ)
		dev_err(priv->dev, "PDMA read Descriptor error!\n");

	if (priv->dma_status == DMA_CCR_EVT_DATA_RW)
		dev_err(priv->dev,
			"PDMA read/write memory descriptor error!\n");

	if (priv->dma_status == DMA_CCR_EVT_EARLY_END)
		dev_err(priv->dev, "PDMA read early end!\n");

	return 0;
}

static int wmt_nand_write_oob(struct mtd_info *mtd, struct nand_chip *chip,
			    int page)
{
	int addr_cycle;
	int status;
	struct nand_priv *priv = wmt_mtd_to_priv(mtd);

	chip->cmdfunc(mtd, NAND_CMD_READ0, 0, page);
	priv->dataptr = 0;
	addr_cycle = wmt_nand_set_addr(priv, 0, page);
	wmt_nand_setup_dma_transfer(priv, NAND_DIR_WRITE);

	memset_io(priv->nand.IO_ADDR_R + REG_ECC_FIFO_0, 0xff, 64);
	memcpy_toio(priv->nand.IO_ADDR_R + REG_ECC_FIFO_0, chip->oob_poi,
		    min(24u, mtd->oobsize));

	writeb(NAND_CMD_SEQIN, priv->nand.IO_ADDR_W);
	wmt_nand_trigger_command(priv, COMCTRL_NFC_2_NAND,
			       addr_cycle + 1);

	wmt_nand_wait_dma(priv);

	wmt_nand_wait_transfer_ready(priv);

	chip->cmdfunc(mtd, NAND_CMD_PAGEPROG, -1, -1);

	status = chip->waitfunc(mtd, chip);

	return status & NAND_STATUS_FAIL ? -EIO : 0;

}

static int wmt_nand_write_page(struct mtd_info *mtd, struct nand_chip *chip,
				const uint8_t *buf, int oob_required)
{
	int addr_cycle;
	struct nand_priv *priv = wmt_mtd_to_priv(mtd);

	memset_io(priv->nand.IO_ADDR_R + REG_ECC_FIFO_0, 0xff, 64);
	memcpy_toio(priv->nand.IO_ADDR_R + REG_ECC_FIFO_0, chip->oob_poi,
		    min(24u, mtd->oobsize));

	priv->dataptr = 0;
	chip->write_buf(mtd, buf, mtd->writesize);

	addr_cycle = wmt_nand_set_addr(priv, 0, priv->page);

	wmt_nand_setup_dma_transfer(priv, NAND_DIR_WRITE);

	writeb(NAND_CMD_SEQIN, priv->nand.IO_ADDR_W);
	wmt_nand_trigger_command(priv, COMCTRL_NFC_2_NAND,
			       addr_cycle + 1);

	wmt_nand_wait_dma(priv);

	wmt_nand_wait_transfer_ready(priv);

	return 0;
}

static int wmt_nand_read_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
				   uint8_t *buf, int oob_required, int page)
{
	struct nand_priv *priv = wmt_mtd_to_priv(mtd);

	chip->read_buf(mtd, buf, mtd->writesize);
	memcpy_fromio(chip->oob_poi, priv->nand.IO_ADDR_R + REG_ECC_FIFO_0,
		      min(64u, mtd->oobsize));

	return 0;
}

static void wmt_nand_select_chip(struct mtd_info *mtd, int chipnr)
{

	struct nand_priv *priv = wmt_mtd_to_priv(mtd);

	dev_dbg(priv->dev, "Selecting chip nr %d", chipnr);

	switch (chipnr) {
	case -1:
		writeb(0xff, priv->nand.IO_ADDR_R + REG_CHIP_ENABLE_CTRL);
		break;
	case 0:
		writeb(0xfe, priv->nand.IO_ADDR_R + REG_CHIP_ENABLE_CTRL);
		break;
	default:
		dev_err(priv->dev, "Only one chip nr 0 is supported, got chip nr:%d", chipnr);
		BUG();
		break;
	}
}

static void wmt_nand_read_resume(struct nand_priv *priv)
{
	wmt_nand_set_bit(priv, REG_ECC_BCH_CTRL, EBC_READ_RESUME);
}

static u8 wmt_nand_bit_correct(u8 val, int bit)
{
	return val ^ (1 << bit);
}

static void wmt_nand_correct_error(struct nand_priv *priv)
{
	int i, err_count, err_byte, err_bit, err_reg, oob, bank, err_idx;
	u8 v;

	err_count = readl(priv->nand.IO_ADDR_R + REG_ECC_BCH_INT_STAT2) & 0xf;
	oob = wmt_nand_get_bit(priv, REG_ECC_BCH_INT_STAT2, EBIS2_ERROR_OOB);

	if (!oob)
		bank = (readl(priv->nand.IO_ADDR_R + REG_ECC_BCH_INT_STAT2) >> 8) & 0x3;

	if (err_count > 4) {
		dev_info(priv->dev, "Too many errors(%d), cannot correct.\n", err_count);
		priv->mtd.ecc_stats.failed++;
		wmt_nand_read_resume(priv);
		return;
	}

	for (i = 0; i < err_count; i++) {
		err_reg = readl(priv->nand.IO_ADDR_R + REG_ECC_BCH_ERR_POS1 + 4 * (i / 2));
		if (i % 2)
			err_reg >>= 16;
		err_reg &= 0x1fff;
		err_byte = err_reg >> 3;
		err_bit = err_reg & 0x7;

		dev_info(priv->dev, "Correcting byte: %d, bit: %d\n", err_byte, err_bit);

		if (!oob) {
			err_idx = priv->nand.ecc.size * bank + err_byte;
			v = priv->dmabuf[err_idx];
			v = wmt_nand_bit_correct(v, err_bit);
			priv->dmabuf[err_idx] = v;
		} else {
			v = readb(priv->nand.IO_ADDR_R + REG_ECC_FIFO_0 + err_byte);
			v = wmt_nand_bit_correct(v, err_bit);
			writeb(v, priv->nand.IO_ADDR_R + REG_ECC_FIFO_0 + err_byte);
		}
	}

	wmt_nand_read_resume(priv);
}

static int wmt_nand_wait_completion(struct nand_priv *priv)
{
	if (!wait_for_completion_interruptible_timeout
	    (&priv->nand_complete, msecs_to_jiffies(1000))) {
		dev_err(priv->dev, "Waiting for nand interrupt failed!\n");
		return -1;
	}

	return 0;
}

static void wmt_nand_read_command(struct nand_priv *priv, int page_addr,
				int column, int command)
{
	int addr_cycle;
	unsigned short tmp;

	if (command == NAND_CMD_READOOB && column != -1)
		column +=
		    (priv->nand.ecc.size +
		     priv->nand.ecc.bytes) * priv->nand.ecc.steps;

	addr_cycle = wmt_nand_set_addr(priv, column, page_addr);

	if (command == NAND_CMD_READ0)
		wmt_nand_setup_dma_transfer(priv, NAND_DIR_READ);

	tmp = readw(priv->nand.IO_ADDR_R + REG_ECC_BCH_INT_STAT1);
	tmp |= EBIS1_ERROR | EBIS1_CORRECTION_DONE;
	writew(tmp, priv->nand.IO_ADDR_R + REG_ECC_BCH_INT_STAT1);

	priv->dataptr = 0;

	writeb(NAND_CMD_READ0, priv->nand.IO_ADDR_W);
	writeb(NAND_CMD_READSTART,
	       wmt_nand_addr_cycle_to_reg(priv, addr_cycle));
	wmt_nand_trigger_command(priv, COMCTRL_NAND_2_NFC |
				 COMCTRL_MULT_COMMANDS, addr_cycle + 2);

	if (command == NAND_CMD_READ0)
		wmt_nand_wait_dma(priv);

	wmt_nand_wait_completion(priv);
}

static void wmt_nand_readid(struct nand_priv *priv, int column)
{
	int addr_cycle = 0;
	int i;

	addr_cycle = wmt_nand_set_addr(priv, 0, -1);
	writeb(NAND_CMD_READID, priv->nand.IO_ADDR_W);

	wmt_nand_trigger_command(priv, COMCTRL_NO_DATA | COMCTRL_NFC_2_NAND |
				 COMCTRL_CYCLES_NONE, addr_cycle + 1);
	wmt_nand_wait_cmd_ready(priv);

	for (i=0; i < column + 8; i++) {
		wmt_nand_trigger_command(priv, COMCTRL_HAS_DATA |
					 COMCTRL_NAND_2_NFC |
					 COMCTRL_CYCLES_SINGLE, 0);

		if (wmt_nand_wait_cmd_ready(priv))
			dev_warn(priv->dev, "Timed out waiting for command completion before reading byte\n");

		wmt_nand_wait_transfer_ready(priv);

		priv->dmabuf[i] = readb(priv->nand.IO_ADDR_R);
	}

	priv->dataptr = column;
}

static void wmt_nand_cmdfunc(struct mtd_info *mtd, unsigned command,
				int column, int page_addr)
{

	struct nand_priv *priv = wmt_mtd_to_priv(mtd);
	int addr_cycle = 0;

	dev_dbg(priv->dev, "Command: %u, column: %x, page_addr: %x\n",
		command, column, page_addr);

	switch (command) {
	case NAND_CMD_SEQIN:
		priv->page = page_addr;
		return;
	case NAND_CMD_READID:
		wmt_nand_readid(priv, column);
		return;
	case NAND_CMD_ERASE1:
		addr_cycle = wmt_nand_set_addr(priv, column, page_addr);
	case NAND_CMD_RESET:
	case NAND_CMD_ERASE2:
	case NAND_CMD_PAGEPROG:
	case NAND_CMD_STATUS:
		writeb(command, priv->nand.IO_ADDR_W);

		wmt_nand_trigger_command(priv, COMCTRL_NO_DATA |
					 COMCTRL_NFC_2_NAND |
					 COMCTRL_CYCLES_NONE, addr_cycle + 1);
		if (command == NAND_CMD_ERASE1 || command == NAND_CMD_STATUS
					       || command == NAND_CMD_READID)
			wmt_nand_wait_cmd_ready(priv);
		else
			wmt_nand_wait_completion(priv);

		break;
	case NAND_CMD_READOOB:
	case NAND_CMD_READ0:
		wmt_nand_read_command(priv, page_addr, column, command);
		break;
	default:
		dev_err(priv->dev,
			"Command: %u, column: %d, page_addr: %d\n", command,
			column, page_addr);
		BUG();
	}
}

static int wmt_nand_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
			      int page)
{

	struct nand_priv *priv = wmt_mtd_to_priv(mtd);
	uint8_t *buf = chip->oob_poi;

	wmt_nand_set_bit(priv, REG_SMC_ENABLE, 0x02);

	chip->cmdfunc(mtd, NAND_CMD_READOOB, 0, page);

	memcpy_fromio(buf, priv->nand.IO_ADDR_R + REG_ECC_FIFO_0,
		      min(64u, mtd->oobsize));

	wmt_nand_clear_bit(priv, REG_SMC_ENABLE, 0x02);
	return 0;
}

static void wmt_nand_init_chip(struct nand_priv *priv)
{
	unsigned long t;
	u32 page_per_block_div;
	u8 type = TYPESEL_CHECK_ALLFF | TYPESEL_WP_DIS | TYPESEL_DIRECT_MAP |
		  TYPESEL_WIDTH_8 | TYPESEL_DIRECT_MAP;

	switch (priv->mtd.writesize) {
	case 512:
		type |= TYPESEL_PAGE_512;
		break;
	case 2048:
		type |= TYPESEL_PAGE_2K;
		break;
	case 4096:
		type |= TYPESEL_PAGE_4K;
		break;
	case 8192:
		type |= TYPESEL_PAGE_8K;
		break;
	default:
		BUG();
	}

	writeb(type, priv->nand.IO_ADDR_R + REG_NAND_TYPE_SEL);

	switch (priv->mtd.erasesize / priv->mtd.writesize) {
	case 16:
		page_per_block_div = 0;
		break;
	case 32:
		page_per_block_div = 1;
		break;
	case 64:
		page_per_block_div = 2;
		break;
	case 128:
		page_per_block_div = 3;
		break;
	case 256:
		page_per_block_div = 4;
		break;
	case 512:
		page_per_block_div = 5;
		break;
	default:
		BUG();
	}
	printk("mtd pages_per_block = %d\n", (2 ^ (page_per_block_div + 4)));

	t = readl(priv->nand.IO_ADDR_R + REG_PAGESIZE_DIVIDER_SEL);
	t &= ~PAGE_BLOCK_DIVISOR_MASK;
	t |= PAGE_BLOCK_DIVISOR(page_per_block_div);
	writel(t, priv->nand.IO_ADDR_R + REG_PAGESIZE_DIVIDER_SEL);

	/* set ecc type */
	t = readl(priv->nand.IO_ADDR_R + REG_ECC_BCH_CTRL);
	t &= EBC_ECC_TYPE_MASK;

	if (priv->mtd.writesize >= 8192)
		t |= EBC_ECC_24BITPER1K;
	else if (priv->mtd.writesize >= 4096 && priv->mtd.oobsize >= 218)
		t |= EBC_ECC_12BIT;
	else if (priv->mtd.writesize > 512)
		t |= EBC_ECC_4BIT;
	else
		t |= EBC_ECC_1BIT;

	writel(t, priv->nand.IO_ADDR_R + REG_ECC_BCH_CTRL);

	/* enable hardware ecc */
	wmt_nand_clear_bit(priv, REG_MISC_CTRL, MISCCTRL_SOFTWARE_ECC);

	/* enable ecc interrupt */
	writew(EBIM_INT_EN, priv->nand.IO_ADDR_R + REG_ECC_BCH_INT_MASK);
}

static void wmt_nand_write_buf(struct mtd_info *mtd, const uint8_t * buf, int len)
{
	struct nand_priv *priv = wmt_mtd_to_priv(mtd);

	len = min(len, (DMA_BUFFER_SIZE - priv->dataptr));

	memcpy(priv->dmabuf + priv->dataptr, buf, len);
	priv->dataptr += len;
}

static void wmt_nand_read_buf(struct mtd_info *mtd, uint8_t * buf, int len)
{
	struct nand_priv *priv = wmt_mtd_to_priv(mtd);

	len = min(len, (DMA_BUFFER_SIZE - priv->dataptr));

	memcpy(buf, priv->dmabuf + priv->dataptr, len);
	priv->dataptr += len;
}

static uint8_t wmt_nand_read_byte(struct mtd_info *mtd)
{
	uint8_t tmp;

	wmt_nand_read_buf(mtd, &tmp, 1);

	return tmp;
}

static void wmt_nand_startup(struct nand_priv *priv)
{
	wmt_nand_read_resume(priv);
	writel(RESM_MASK, priv->nand.IO_ADDR_R + REG_REDUNT_ECC_STAT_MASK);
}

static irqreturn_t wmt_nand_irq(int irq_num, void *_data)
{
	struct nand_priv *priv = _data;

	int loop_guard = 1 << 20;

	if (wmt_nand_get_bit(priv, REG_ECC_BCH_INT_STAT1, EBIS1_ERROR)) {
		wmt_nand_correct_error(priv);
		return IRQ_HANDLED;
	}

	while (--loop_guard && !wmt_nand_get_busy2ready(priv))
		cpu_relax();

	BUG_ON(!loop_guard);

	wmt_nand_clear_busy2ready(priv);
	complete(&priv->nand_complete);

	return IRQ_HANDLED;
}

static irqreturn_t wmt_nand_dma_irq(int irq_num, void *_data)
{
	struct nand_priv *priv = _data;

	if (readl(priv->nand.IO_ADDR_R + REG_NFC_DMA_ISR) & DMA_IER_INT_STS) {
		priv->dma_status = readl(priv->nand.IO_ADDR_R + REG_NFC_DMA_CCR) & DMA_CCR_EVTCODE;
		writel(DMA_IER_INT_STS, priv->nand.IO_ADDR_R + REG_NFC_DMA_ISR);
	}
	complete(&priv->dma_complete);
	return IRQ_HANDLED;
}

static int wmt_nand_probe(struct platform_device *pdev)
{
	struct nand_priv *priv;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;
	struct mtd_part_parser_data mtd_ppd;
	unsigned int nand_options;
	int ecc_mode, err;

	priv = devm_kzalloc(&pdev->dev, sizeof(struct nand_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &pdev->dev;
	dev_set_drvdata(priv->dev, priv);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	priv->reg_base = devm_ioremap_resource(priv->dev, res);
	if (IS_ERR(priv->reg_base)) {
		dev_err(priv->dev, "Failed to map register memory");
		return PTR_ERR(priv->reg_base);
	}

	priv->clk = of_clk_get(np, 0);
	if (IS_ERR(priv->clk)) {
		dev_err(priv->dev, "Failed to get clock\n");
		return -EINVAL;
	}

	priv->dmabuf = dmam_alloc_coherent(priv->dev, DMA_BUFFER_SIZE,
					   &priv->dmaaddr, GFP_KERNEL);
	if (!priv->dmabuf) {
		dev_err(priv->dev, "Failed to allocate dma memory");
		return -ENOMEM;
	}

	priv->dma_desc = dmam_alloc_coherent(priv->dev,
					     sizeof(*priv->dma_desc),
					     &priv->dma_d_addr, GFP_KERNEL);
	if (!priv->dma_desc) {
		dev_err(priv->dev, "Failed to allocate dma descriptor");
		return -ENOMEM;
	}

	priv->nand_irq = platform_get_irq(pdev, 0);
	if (priv->nand_irq == NO_IRQ) {
		dev_err(priv->dev, "Failed to retrive nand irq");
		return -EINVAL;
	}

	priv->dma_irq = platform_get_irq(pdev, 1);
	if (priv->dma_irq == NO_IRQ) {
		dev_err(priv->dev, "Failed to retrive nand dma irq");
		return -EINVAL;
	}

	if (devm_request_irq(priv->dev, priv->nand_irq, wmt_nand_irq, 0,
			     "nand", priv)) {
		dev_err(priv->dev, "Failed to register nand irq handler");
		return -EINVAL;
	}

	if (devm_request_irq(priv->dev, priv->dma_irq, wmt_nand_dma_irq, 0,
			     "nand-dma", priv)) {
		dev_err(priv->dev, "Failed to register dma irq handler");
		return -EINVAL;
	}

	priv->mtd.priv = &priv->nand;
	priv->mtd.owner = THIS_MODULE;
	priv->mtd.name = "wmt_nand";

	priv->nand.bbt_td = &nand_bbt_main_descr_2048;
	priv->nand.bbt_md = &nand_bbt_mirror_descr_2048;
	
	ecc_mode = of_get_nand_ecc_mode(np);
	priv->nand.ecc.mode = ecc_mode < 0 ? NAND_ECC_SOFT : ecc_mode;
	priv->nand.ecc.layout = &vt8500_oobinfo;
	priv->nand.ecc.size = 512;
	priv->nand.ecc.bytes = 8;
	priv->nand.ecc.steps = 8;
	priv->nand.ecc.strength = 4;

	priv->nand.buffers = devm_kzalloc(priv->dev, sizeof(*priv->nand.buffers), GFP_KERNEL);
	if (!priv->nand.buffers) {
		printk("failed to allocate buffers\n");
		return -ENOMEM;
	}

	nand_options = NAND_OWN_BUFFERS | NAND_BBT_LASTBLOCK | NAND_BBT_PERCHIP;
	if (of_get_nand_on_flash_bbt(np))
		nand_options |= NAND_BBT_USE_FLASH;
	if (of_get_nand_bus_width(np) == 16)
		nand_options |= NAND_BUSWIDTH_16;

	priv->nand.options = nand_options;
	priv->nand.IO_ADDR_R = priv->reg_base;
	priv->nand.IO_ADDR_W = priv->reg_base + REG_COMPORT0;
	priv->nand.cmdfunc = wmt_nand_cmdfunc;
	priv->nand.dev_ready = wmt_nand_device_ready;
	priv->nand.read_byte = wmt_nand_read_byte;
	priv->nand.read_buf = wmt_nand_read_buf;
	priv->nand.write_buf = wmt_nand_write_buf;
	priv->nand.select_chip = wmt_nand_select_chip;
	priv->nand.ecc.read_page = wmt_nand_read_page;
	priv->nand.ecc.read_page_raw = wmt_nand_read_page_raw;
	priv->nand.ecc.read_oob = wmt_nand_read_oob;
	priv->nand.ecc.write_page_raw = wmt_nand_write_page;
	priv->nand.ecc.write_page = wmt_nand_write_page;
	priv->nand.ecc.write_oob = wmt_nand_write_oob;
	priv->nand.chip_delay = 20;

	init_completion(&priv->nand_complete);
	init_completion(&priv->dma_complete);

	clk_prepare_enable(priv->clk);
	wmt_nand_startup(priv);

	//wmt_nand_set_clock(priv);

	err = nand_scan_ident(&priv->mtd, 1, NULL);
	if (err) {
		printk("nand_scan_ident() failed\n");
		clk_disable_unprepare(priv->clk);
		return -ENXIO;
	}

	printk("mtd erasesize = %d\n", priv->mtd.erasesize);
	printk("mtd writesize = %d\n", priv->mtd.writesize);
	wmt_nand_init_chip(priv);

	err = nand_scan_tail(&priv->mtd);
	if (err) {
		printk("nand_scan_tail() failed\n");
		clk_disable_unprepare(priv->clk);
		return -ENXIO;
	}

	mtd_ppd.of_node = np;
	err = mtd_device_parse_register(&priv->mtd, NULL, &mtd_ppd, NULL, 0);
	if (err) {
		printk("mtd_device_register() failed\n");
		nand_release(&priv->mtd);
		return err;
	}

	return 0;
}

static int wmt_nand_remove(struct platform_device *pdev)
{

	struct nand_priv *priv = dev_get_drvdata(&pdev->dev);

	nand_release(&priv->mtd);

	clk_disable_unprepare(priv->clk);

	return 0;
}

static struct of_device_id wmt_nand_dt_ids[] = {
	{ .compatible = "wm,wm8750-nand" },
	{ /* Sentinel */ },
};

static struct platform_driver wmt_nand_driver = {
	.probe = wmt_nand_probe,
	.remove = wmt_nand_remove,
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = wmt_nand_dt_ids,
	}
};

module_platform_driver(wmt_nand_driver);

MODULE_DESCRIPTION("WonderMedia WM8xxx-series NAND Driver");
MODULE_AUTHOR("Tony Prisk");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, wmt_nand_dt_ids);
