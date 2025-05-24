#include <linux/bio.h>
#include <linux/device-mapper.h>
#include <linux/init.h>
#include <linux/module.h>

#define DM_MSG_PREFIX "dmp"

struct dmp_target {
    struct dm_dev *dev;
    sector_t start;
};

static struct dmp_stats {
    atomic64_t read_reqs;
    atomic64_t write_reqs;
    atomic64_t total_reqs;
    atomic64_t read_bytes;
    atomic64_t write_bytes;
    atomic64_t total_bytes;
} stats = {
    .read_reqs   = ATOMIC64_INIT(0),
    .write_reqs  = ATOMIC64_INIT(0),
    .total_reqs  = ATOMIC64_INIT(0),
    .read_bytes  = ATOMIC64_INIT(0),
    .write_bytes = ATOMIC64_INIT(0),
    .total_bytes = ATOMIC64_INIT(0),
};

static struct kobject *dmp_kobj;
static ssize_t volumes_show(struct kobject *k, struct kobj_attribute *a, char *buf);
static struct kobj_attribute volumes_attr = __ATTR_RO(volumes);

static ssize_t volumes_show(struct kobject *k, struct kobj_attribute *a, char *buf) {
    u64 rr = atomic64_read(&stats.read_reqs);
    u64 wr = atomic64_read(&stats.write_reqs);
    u64 tr = atomic64_read(&stats.total_reqs);
    u64 rb = atomic64_read(&stats.read_bytes);
    u64 wb = atomic64_read(&stats.write_bytes);
    u64 tb = atomic64_read(&stats.total_bytes);

    return sysfs_emit(buf,
        "read:\n"
        "\treqs: %llu\n"
        "\tavg size: %llu\n"
        "write:\n"
        "\treqs: %llu\n"
        "\tavg size: %llu\n"
        "total:\n"
        "\treqs: %llu\n"
        "\tavg size: %llu\n", 
                      rr, rr ? rb/rr : 0, 
                      wr, wr ? wb/wr : 0,
                      tr, tr ? tb/tr : 0);
}

static int dmp_ctr(struct dm_target *ti, unsigned int argc, char **argv) {
    printk(KERN_INFO "dmp: dmp_ctr called\n");

    struct dmp_target *dmpt;

	if (argc != 1) {
        printk(KERN_ERR "dmp: invalid number of argument, need 1\n");
		ti->error = "usage: dmp <backing device path>";
		return -EINVAL;
	}
    printk(KERN_INFO "dmp[dmp_ctr]<backing device path>: %s\n", argv[0]);

    dmpt = kzalloc(sizeof(struct dmp_target), GFP_KERNEL);

    if (dmpt == NULL) {
        printk(KERN_ERR "dmp: dmpt is null\n");
        ti->error = "dmp: cannot allocate linear context";
        return -ENOMEM;
    }

    dmpt->start = ti->begin;

    if (dm_get_device(ti, argv[0], dm_table_get_mode(ti->table), &dmpt->dev)) {
        ti->error = "dmp: device lookup failed";
        goto dmp_ctr_error;
    }

    ti->private = dmpt;

	return 0;

dmp_ctr_error:
    kfree(dmpt);
    printk(KERN_ERR "dmp: dmp_ctr ERROR\n");
    return -EINVAL;
}

static int dmp_map(struct dm_target *ti, struct bio *bio) {
    struct dmp_target *dmpt = ti->private;
    u64 request_size = bio->bi_iter.bi_size;

    atomic64_inc(&stats.total_reqs);
    atomic64_add(request_size, &stats.total_bytes);

    switch (bio_op(bio)) {
        case REQ_OP_READ:
            if (bio->bi_opf & REQ_RAHEAD)
                return DM_MAPIO_KILL;
            
            atomic64_inc(&stats.read_reqs);
            atomic64_add(request_size, &stats.read_bytes);
            break;

        case REQ_OP_WRITE:
            atomic64_inc(&stats.write_reqs);
            atomic64_add(request_size, &stats.write_bytes);
            break;

        case REQ_OP_DISCARD:
            break;

        default:
            return DM_MAPIO_KILL;
    }

    bio_set_dev(bio, dmpt->dev->bdev);
    submit_bio_noacct(bio);

    return DM_MAPIO_SUBMITTED;
}

static void dmp_io_hints(struct dm_target *ti, struct queue_limits *limits) {
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
	.version = {1, 0, 1},
	.features = DM_TARGET_NOWAIT,
	.module = THIS_MODULE,
	.ctr    = dmp_ctr,
	.dtr    = dmp_dtr,
	.map    = dmp_map,
	.io_hints = dmp_io_hints,
};

static int __init dmp_init(void) {
    int return_value;

    dmp_kobj = kobject_create_and_add("stat", &THIS_MODULE->mkobj.kobj);
    if (!dmp_kobj) return -ENOMEM;

    return_value = sysfs_create_file(dmp_kobj, &volumes_attr.attr);
    if (return_value) goto err_kobj;

    return_value = dm_register_target(&dmp_target);
    if (return_value) goto err_sysfs;

    pr_info("dmp: module loaded\n");
    return 0;

err_sysfs:
    sysfs_remove_file(dmp_kobj, &volumes_attr.attr);

err_kobj:
    kobject_put(dmp_kobj);

    return return_value;
}

static void __exit dmp_exit(void) {
    dm_unregister_target(&dmp_target);
    sysfs_remove_file(dmp_kobj, &volumes_attr.attr);
    kobject_put(dmp_kobj);
    pr_info("dmp: module unloaded\n");
}

module_init(dmp_init);
module_exit(dmp_exit);
MODULE_LICENSE("GPL");
