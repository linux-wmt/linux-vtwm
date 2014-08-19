/*
 * Copyright (C) 2014 Alexey Charkov <alchark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <asm/cacheflush.h>
#include <asm/delay.h>
#include <asm/smp.h>
#include <asm/smp_plat.h>
#include <asm/smp_scu.h>

extern void wmt_secondary_startup(void);

/* ioremap is not yet available early enough for init_cpus */
static void __iomem *scu_base = IOMEM(0xf8018000);

static DEFINE_SPINLOCK(boot_lock);

static void write_pen_release(int val)
{
	pen_release = val;
	smp_wmb();
	sync_cache_w(&pen_release);
}

static void wmt_secondary_init(unsigned int cpu)
{
	/*
	 * let the primary processor know we're out of the
	 * pen, then head off into the C entry point
	 */
	write_pen_release(-1);

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}

static int wmt_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	unsigned long timeout;

	/*
	 * Set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);
	write_pen_release(cpu_logical_map(cpu));

	arch_send_wakeup_ipi_mask(cpumask_of(cpu));

	timeout = jiffies + (1 * HZ);
	while (time_before(jiffies, timeout)) {
		smp_rmb();
		if (pen_release == -1)
			break;

		udelay(10);
	}

	spin_unlock(&boot_lock);

	return 0;
}

static void __init wmt_init_cpus(void)
{
	unsigned int i, ncores;

	ncores = scu_get_core_count(scu_base);

	/* sanity check */
	if (ncores > nr_cpu_ids) {
		pr_warn("SMP: %u cores greater than maximum (%u), clipping\n",
			ncores, nr_cpu_ids);
		ncores = nr_cpu_ids;
	}

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);
}

static void __init wmt_smp_prepare_cpus(unsigned int max_cpus)
{
	struct device_node *np;
	void __iomem *secondary_vector_base;

	np = of_find_compatible_node(NULL, NULL, "arm,cortex-a9-scu");
	scu_base = of_iomap(np, 0);
	of_node_put(np);

	if (!scu_base)
		return;

	np = of_find_compatible_node(NULL, NULL, "wm,secondary-cpu-vector");
	secondary_vector_base = of_iomap(np, 0);
	of_node_put(np);

	if (!secondary_vector_base)
		goto unmap_scu;

	scu_enable(scu_base);

	writel(virt_to_phys(wmt_secondary_startup), secondary_vector_base);

	iounmap(secondary_vector_base);
unmap_scu:
	iounmap(scu_base);
}

struct smp_operations wmt_smp_ops __initdata = {
	.smp_secondary_init	= wmt_secondary_init,
	.smp_init_cpus		= wmt_init_cpus,
	.smp_prepare_cpus	= wmt_smp_prepare_cpus,
	.smp_boot_secondary	= wmt_boot_secondary,
};
CPU_METHOD_OF_DECLARE(wmt_prizm_smp, "wm,prizm-smp", &wmt_smp_ops);
