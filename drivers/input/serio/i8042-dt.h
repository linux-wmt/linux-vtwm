#ifndef _I8042_DT_H
#define _I8042_DT_H

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

/*
 * Names.
 */

static void __iomem *dt_base;
static const char *dt_kbd_phys_desc;
static const char *dt_aux_phys_desc;
static const char *dt_mux_phys_desc;
static int dt_kbd_irq;
static int dt_aux_irq;
static unsigned int dt_command_reg;
static unsigned int dt_status_reg;
static unsigned int dt_data_reg;

#define I8042_KBD_PHYS_DESC	dt_kbd_phys_desc
#define I8042_AUX_PHYS_DESC	dt_aux_phys_desc
#define I8042_MUX_PHYS_DESC	dt_mux_phys_desc

#define I8042_KBD_IRQ		(dt_kbd_irq)
#define I8042_AUX_IRQ		(dt_aux_irq)

#define I8042_COMMAND_REG	(dt_command_reg)
#define I8042_STATUS_REG	(dt_status_reg)
#define I8042_DATA_REG		(dt_data_reg)


static inline int i8042_read_data(void)
{
	return readb(dt_base + dt_data_reg);
}

static inline int i8042_read_status(void)
{
	return readb(dt_base + dt_status_reg);
}

static inline void i8042_write_data(int val)
{
	writeb(val, dt_base + dt_data_reg);
}

static inline void i8042_write_command(int val)
{
	writeb(val, dt_base + dt_command_reg);
}

static inline int dt_parse_node(struct device_node *np)
{
	int ret;

	dt_base = of_iomap(np, 0);
	if (!dt_base)
		return -ENOMEM;

	ret = of_property_read_u32(np, "command-reg", &dt_command_reg);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "status-reg", &dt_status_reg);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "data-reg", &dt_data_reg);
	if (ret)
		return ret;

	dt_kbd_irq = irq_of_parse_and_map(np, 0);
	dt_aux_irq = irq_of_parse_and_map(np, 1);

	ret = of_property_read_string(np, "linux,kbd_phys_desc",
					&dt_kbd_phys_desc);
	if (ret)
		dt_kbd_phys_desc = "i8042/serio0";

	ret = of_property_read_string(np, "linux,aux_phys_desc",
					&dt_aux_phys_desc);
	if (ret)
		dt_aux_phys_desc = "i8042/serio1";

	ret = of_property_read_string(np, "linux,mux_phys_desc",
					&dt_mux_phys_desc);
	if (ret)
		dt_mux_phys_desc = "i8042/serio%d";

	if (of_get_property(np, "init-reset", NULL))
		i8042_reset = true;

	return 0;
}

static inline int i8042_platform_init(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "intel,8042");
	if (!np) {
		pr_err("%s: no devicetree node found\n", __func__);
		return -ENODEV;
	}

	dt_parse_node(np);

	return 0;
}

static inline void i8042_platform_exit(void)
{
	if (dt_base)
		iounmap(dt_base);
}

#endif
