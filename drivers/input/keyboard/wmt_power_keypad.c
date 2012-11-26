/* Wondermedia Power Keypad
 *
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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/bitops.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>

static void __iomem *pmc_base;
static struct input_dev *kpad_power;
static spinlock_t kpad_power_lock;
static int power_button_pressed;
static struct timer_list kpad_power_timer;
static unsigned int kpad_power_code;

static inline void kpad_power_timeout(unsigned long fcontext)
{
	u32 status;
	unsigned long flags;

	spin_lock_irqsave(&kpad_power_lock, flags);

	status = readl(pmc_base + 0x14);

	if (power_button_pressed) {
		input_report_key(kpad_power, kpad_power_code, 0);
		input_sync(kpad_power);
		power_button_pressed = 0;
	}

	spin_unlock_irqrestore(&kpad_power_lock, flags);
}

static irqreturn_t kpad_power_isr(int irq_num, void *data)
{
	u32 status;
	unsigned long flags;

	spin_lock_irqsave(&kpad_power_lock, flags);

	status = readl(pmc_base + 0x14);
	udelay(100);
	writel(status, pmc_base + 0x14);

	if (status & BIT(14)) {
		if (!power_button_pressed) {
			input_report_key(kpad_power, kpad_power_code, 1);
			input_sync(kpad_power);

			power_button_pressed = 1;

			mod_timer(&kpad_power_timer,
				  jiffies + msecs_to_jiffies(500));
		}
	}

	spin_unlock_irqrestore(&kpad_power_lock, flags);

	return IRQ_HANDLED;
}

static int vt8500_pwr_kpad_probe(struct platform_device *pdev)
{
	struct device_node *np;
	u32 status;
	int err;
	int irq;

	np = of_find_compatible_node(NULL, NULL, "via,vt8500-pmc");
	if (!np) {
		dev_err(&pdev->dev, "pmc node not found\n");
		return -EINVAL;
	}

	pmc_base = of_iomap(np, 0);
	if (!pmc_base) {
		dev_err(&pdev->dev, "unable to map pmc memory\n");
		return -ENOMEM;
	}

	np = pdev->dev.of_node;
	if (!np) {
		dev_err(&pdev->dev, "devicenode not found\n");
		return -ENODEV;
	}

	err = of_property_read_u32(np, "keymap", &kpad_power_code);
	if (err) {
		dev_warn(&pdev->dev, "defaulting to KEY_POWER\n");
		kpad_power_code = KEY_POWER;
	}

	/* set power button to soft-power */
	status = readl(pmc_base + 0x54);
	writel(status | 1, pmc_base + 0x54);

	/* clear any pending interrupts */
	status = readl(pmc_base + 0x14);
	writel(status, pmc_base + 0x14);

	kpad_power = input_allocate_device();
	if (!kpad_power) {
		dev_err(&pdev->dev, "failed to allocate input device\n");
		return -ENOMEM;
	}

	spin_lock_init(&kpad_power_lock);
	setup_timer(&kpad_power_timer, kpad_power_timeout,
				       (unsigned long)kpad_power);

	irq = irq_of_parse_and_map(np, 0);
	err = request_irq(irq, &kpad_power_isr, 0, "pwrbtn", NULL);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to request irq\n");
		return err;
	}

	kpad_power->evbit[0] = BIT_MASK(EV_KEY);
	set_bit(kpad_power_code, kpad_power->keybit);

	kpad_power->name = "wmt_power_keypad";
	kpad_power->phys = "wmt_power_keypad";
	kpad_power->keycode = &kpad_power_code;
	kpad_power->keycodesize = sizeof(unsigned int);
	kpad_power->keycodemax = 1;

	err = input_register_device(kpad_power);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to register input device\n");
		return err;
	}

	return 0;
}

static struct of_device_id vt8500_pwr_kpad_dt_ids[] = {
	{ .compatible = "wm,power-keypad" },
	{ /* Sentinel */ },
};

static struct platform_driver vt8500_pwr_kpad_driver = {
	.probe		= vt8500_pwr_kpad_probe,
	.driver		= {
		.name	= "wmt-power-keypad",
		.owner	= THIS_MODULE,
		.of_match_table = vt8500_pwr_kpad_dt_ids,
	},
};

module_platform_driver(vt8500_pwr_kpad_driver);

MODULE_DESCRIPTION("Wondermedia Power Keypad Driver");
MODULE_AUTHOR("Tony Prisk <linux@prisktech.co.nz>");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, vt8500_pwr_kpad_dt_ids);
