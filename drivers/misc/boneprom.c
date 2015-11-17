#include<linux/i2c.h>
#include<linux/module.h>
#include<linux/types.h>
#include<linux/delay.h>
#include<linux/byteorder/generic.h>

static char *cmd = "rd";
static int addr;
static char data[10];
static int len;
static int data_len;

module_param(cmd, charp, 0);
module_param(addr, int, 0);
module_param(len, int, 0);
module_param_array(data, byte, &data_len, 0);

static const struct i2c_device_id boneprom_i2c_id[] = {
        {"eeprom", 0},
        {},
};
MODULE_DEVICE_TABLE(i2c, boneprom_i2c_id);

static const struct of_device_id boneprom_ids[] = {
        { .compatible = "beaglebone,eeprom", .data = NULL },
        {},
};

/* Creates an alias for the id table, automatically stored in kernel module */
MODULE_DEVICE_TABLE(of, boneprom_ids);

static int boneprom_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	int bytes, i;
	__be16 be_addr;
	char write_buf[12];

	be_addr = cpu_to_be16(addr);

	if (!strncmp(cmd, "wr", 2)) {
		/*
		 * For writes, the address is sent first and then the data
		 * all in the same i2c command sequence.
		 */

		/* Copy address to the write buffer */
		write_buf[0] = be_addr & (0xff); // LSB
		write_buf[1] = be_addr >> 8;	 // MSB

		/* Copy the data to the write buffer */
		memcpy(write_buf+2, data, len);

		/* Send it all at once */
		bytes = i2c_master_send(i2c, write_buf, len + 2);
		bytes -= 2;
	} else {
		/*
		 * For reads, the address first needs to be sent
		 * and then the data is received. Thus, 2 i2c command
		 * sequences are required.
		 */

		/* Write the address */
		if(i2c_master_send(i2c, (char *)(&be_addr), 2) != 2) {
			pr_err("Couldn't send address bytes\n");
			return -1;
		}

		/* Receive the data */
		bytes = i2c_master_recv(i2c, data, len);
		for (i = 0; i < len; i++) {
			printk("[%x] %2x\n", addr + i, data[i]);
		}
	}

	if (bytes != len)
		pr_err("Not all bytes sent/received (%d)\n", bytes);
	else
		pr_err("All bytes sent/received\n");

	return -1;
}

static int boneprom_remove(struct i2c_client *i2c) {
	return -1;
}

static struct i2c_driver boneprom_driver = {
        .driver = {
                .name = "boneprom",
                .owner = THIS_MODULE,
                .of_match_table = of_match_ptr(boneprom_ids),
        },
        .id_table = boneprom_i2c_id,
        .probe = boneprom_probe,
	.remove = boneprom_remove,
};

static int __init boneprom_init(void)
{
	if (strncmp(cmd, "rd", 2) && strncmp(cmd, "wr", 2)) {
		pr_err("Invalid command, pass rd/wr\n");
		return -1;
	}

	if (len <= 0 || len > 10) {
		pr_err("len should be 1 to 10\n");
		return -1;
	}

	if (!strncmp(cmd, "wr", 2) && data_len != len) {
		pr_err("Invalid length of data for write\n");
		return -1;
	}

	/*
	 * Register driver with i2c core, this will probe and send commands.
	 */
	if (i2c_add_driver(&boneprom_driver)) {
		pr_err("i2c driver add failed\n");
		return -1;
	}

	pr_err("Removing driver\n");

	/* De-register driver from i2c core */
	i2c_del_driver(&boneprom_driver);
	return -1;
}

module_init(boneprom_init);

MODULE_AUTHOR("Joel Fernandes <joel@linuxinternals.org>");
MODULE_DESCRIPTION("i2c kernel driver demo");
MODULE_LICENSE("GPL v2");
