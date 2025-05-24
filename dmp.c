#include <linux/device-mapper.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/bio.h>

#define DM_MSG_PREFIX "dmp"

struct dmp_target {
    struct dm_dev *dev;
    sector_t start;
};

static int dmp_ctr(struct dm_target *ti, unsigned int argc, char **argv) {
    printk(KERN_INFO "dmp: dmp_ctr called\n");

    struct dmp_target *dmpt;

	if (argc != 1) {
        printk(KERN_ERR "dmp: invalid number of argument, need 1\n");
		ti->error = "usage: dmp <backing device path>";
		return -EINVAL;
	}
    printk(KERN_INFO "dmp[dmp_ctr]<backing device path>: %s\n", argv[0]);

    dmpt = kmalloc(sizeof(struct dmp_target), GFP_KERNEL);

    if (dmpt == NULL) {
        printk(KERN_ERR "dmp: dmpt is null\n");
        ti->error = "dmp: cannot allocate linear context";
        return -ENOMEM;
    }

    dmpt->start = ti->begin;

    if (dm_get_device(ti, argv[0], dm_table_get_mode(ti->table), &dmpt->dev)) {
        ti->error = "dmp: device lookup failed";
        goto bad;
    }

    ti->private = dmpt;

	return 0;

bad:
    kfree(dmpt);
    printk(KERN_ERR "dmp: dmp_ctr ERROR\n");
    return -EINVAL;
}

static int dmp_map(struct dm_target *ti, struct bio *bio)
{
	switch (bio_op(bio)) {
	case REQ_OP_READ:
		if (bio->bi_opf & REQ_RAHEAD) {
			/* readahead of null bytes only wastes buffer cache */
			return DM_MAPIO_KILL;
		}
		zero_fill_bio(bio);
		break;
	case REQ_OP_WRITE:
	case REQ_OP_DISCARD:
		/* writes get silently dropped */
		break;
	default:
		return DM_MAPIO_KILL;
	}

	bio_endio(bio);

	/* accepted bio, don't make new request */
	return DM_MAPIO_SUBMITTED;
}

static void dmp_io_hints(struct dm_target *ti, struct queue_limits *limits)
{
	limits->max_hw_discard_sectors = UINT_MAX;
	limits->discard_granularity = 512;
}

static void dmp_dtr(struct dm_target *ti) {
    printk(KERN_INFO "dmp: dmp_dtr called\n");
    struct dmp_target *dmpt = (struct dmp_target*)ti->private;
    dm_put_device(ti, dmpt->dev);
    kfree(dmpt);
}

static struct target_type dmp_target = {
	.name   = "dmp",
	.version = {1, 2, 0},
	.features = DM_TARGET_NOWAIT,
	.module = THIS_MODULE,
	.ctr    = dmp_ctr,
	.dtr    = dmp_dtr,
	.map    = dmp_map,
	.io_hints = dmp_io_hints,
};
module_dm(dmp);

MODULE_LICENSE("GPL");
