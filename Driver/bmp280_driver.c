/* Author: K.Shelby IOT - VTN */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>  

/* Calibration registers */
#define BMP280_REG_DIG_T1   0x88
#define BMP280_REG_DIG_T2   0x8A
#define BMP280_REG_DIG_T3   0x8C
#define BMP280_REG_DIG_P1   0x8E
#define BMP280_REG_DIG_P2   0x90
#define BMP280_REG_DIG_P3   0x92
#define BMP280_REG_DIG_P4   0x94
#define BMP280_REG_DIG_P5   0x96
#define BMP280_REG_DIG_P6   0x98
#define BMP280_REG_DIG_P7   0x9A
#define BMP280_REG_DIG_P8   0x9C
#define BMP280_REG_DIG_P9   0x9E

/* Config register */
#define BMP280_REG_CONTROL  0xF4
#define BMP280_REG_CONFIG   0xF5

/* Data registers */
#define TEMP_XLSB_REG   0xFC
#define TEMP_LSB_REG    0xFB
#define TEMP_MSB_REG    0xFA
#define PRESS_XLSB_REG  0xF9
#define PRESS_LSB_REG   0xF8
#define PRESS_MSB_REG   0xF7

#define DRIVER_NAME "bmp280_driver"
#define DEVICE_NAME "bmp280"
#define CLASS_NAME  "bmp280"

static struct i2c_client *bmp280_client;
static struct class *bmp280_class = NULL;
static struct device *bmp280_device = NULL;
static int major_number;

/* Ioctl commands */
#define BMP280_IOCTL_MAGIC 'm'
#define BMP280_IOCTL_READ_TEMPERATURE _IOR(BMP280_IOCTL_MAGIC, 1, int)
#define BMP280_IOCTL_READ_PRESSURE _IOR(BMP280_IOCTL_MAGIC, 2, int)
#define BMP280_IOCTL_WRITE_CONTROL _IOR(BMP280_IOCTL_MAGIC, 3, int)
#define BMP280_IOCTL_WRITE_CONFIG _IOR(BMP280_IOCTL_MAGIC, 4, int)

static int bmp280_read_data(struct i2c_client *client, int num)
{
    uint16_t dig_T1, dig_P1;
    int16_t dig_T2, dig_T3, dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    int32_t pressure_raw, temperature_raw;
    int32_t var1, var2, t_fine;
    int32_t temperature, pressure;
    uint32_t p;
    int buf[6];
    int32_t data[2];

    for (int i = 0; i < 6; i++) {
        buf[i] = i2c_smbus_read_byte_data(client, PRESS_MSB_REG + i);
        if (buf[i] < 0) {
            printk(KERN_ERR "Failed to read sensor data\n");
            break;
            return -EIO;
        }
    }
    pressure_raw = (buf[0] << 12) | (buf[1] << 4) | (buf[2]>>4);
    temperature_raw = (buf[3] << 12) | (buf[4] << 4) | (buf[5]>>4);
    /* Calibration data */ 
    dig_T1 = i2c_smbus_read_word_data(client, BMP280_REG_DIG_T1);
    dig_T2 = i2c_smbus_read_word_data(client, BMP280_REG_DIG_T2);
    dig_T3 = i2c_smbus_read_word_data(client, BMP280_REG_DIG_T3);
    dig_P1 = i2c_smbus_read_word_data(client, BMP280_REG_DIG_P1);
    dig_P2 = i2c_smbus_read_word_data(client, BMP280_REG_DIG_P2);
    dig_P3 = i2c_smbus_read_word_data(client, BMP280_REG_DIG_P3);
    dig_P4 = i2c_smbus_read_word_data(client, BMP280_REG_DIG_P4);
    dig_P5 = i2c_smbus_read_word_data(client, BMP280_REG_DIG_P5);
    dig_P6 = i2c_smbus_read_word_data(client, BMP280_REG_DIG_P6);
    dig_P7 = i2c_smbus_read_word_data(client, BMP280_REG_DIG_P7);
    dig_P8 = i2c_smbus_read_word_data(client, BMP280_REG_DIG_P8);
    dig_P9 = i2c_smbus_read_word_data(client, BMP280_REG_DIG_P9);

    /* Calculate temperature */
    var1 = ((((temperature_raw >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    var2 = (((((temperature_raw >> 4) - ((int32_t)dig_T1)) * ((temperature_raw >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    temperature = (t_fine * 5 + 128) >> 8;

    /* Calculate pressure */
    var1 = (t_fine >> 1) - 64000;
    var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t)dig_P6);
    var2 = var2 + ((var1 * ((int32_t)dig_P5)) << 1);
    var2 = (var2 >> 2) + (((int32_t)dig_P4) << 16);
    var1 = (((dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) + ((((int32_t)dig_P2) * var1) >> 1)) >> 18;
    var1 = ((((32768 + var1)) * ((int32_t)dig_P1)) >> 15);

    if (var1 == 0) {
        pressure = 0;  
    } else {
        p = (((1048576 - pressure_raw) - (var2 >> 12)) * 3125);
        if (p < 0x80000000) {
            p = (p << 1) / ((uint32_t)var1);
        } else {
            p = (p / (uint32_t)var1) * 2;
        }
        var1 = (((int32_t)dig_P9) * ((int32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
        var2 = (((int32_t)(p >> 2)) * ((int32_t)dig_P8)) >> 13;
        p = (uint32_t)((int32_t)p + ((var1 + var2 + dig_P7) >> 4));
        pressure = p;
    }
    data[0] = temperature;
    data[1] = pressure;

    return data[num];
}

static int bmp280_write_data(struct i2c_client *client, int cmd, int data) {
    int ret;
    switch (cmd) {
        case BMP280_IOCTL_WRITE_CONTROL:
            ret = i2c_smbus_write_byte_data(client, BMP280_REG_CONTROL, data);
            if (ret < 0) {
                dev_err(&client->dev, "Failed to write to BMP280_REG_CONTROL register\n");
                return ret;
            }
            break;
        case BMP280_IOCTL_WRITE_CONFIG:
            ret = i2c_smbus_write_byte_data(client, BMP280_REG_CONFIG, data);
            if (ret < 0) {
                dev_err(&client->dev, "Failed to write to BMP280_REG_CONFIG register\n");
                return ret;
            }
            break;
        default:
            dev_err(&client->dev, "Invalid command\n");
            return -EINVAL;
    }
    return 0;
}

static long bmp280_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret;

    switch (cmd) {
        /* Read requests */
        case BMP280_IOCTL_READ_TEMPERATURE:
            ret = bmp280_read_data(bmp280_client, 0);
            if (ret < 0) {
                printk(KERN_ERR "Failed to read temperature data\n");
                return ret;
            }
            if (copy_to_user((int __user *)arg, &ret, sizeof(ret))) {
                return -EFAULT;
            }
            break;
        case BMP280_IOCTL_READ_PRESSURE:
            ret = bmp280_read_data(bmp280_client, 1);
            if (ret < 0) {
                printk(KERN_ERR "Failed to read pressure data\n");
                return ret;
            }       
            if (copy_to_user((int __user *)arg, &ret, sizeof(ret))) {
                return -EFAULT;
            }
            break;
        /* Write requests */
        case BMP280_IOCTL_WRITE_CONTROL:
            ret = bmp280_write_data(bmp280_client, BMP280_IOCTL_WRITE_CONTROL, arg);
            if (ret < 0) {
                printk(KERN_ERR "Failed to write to BMP280_REG_CONTROL register\n");
                return ret;
            }
            break;
        case BMP280_IOCTL_WRITE_CONFIG:
            ret = bmp280_write_data(bmp280_client, BMP280_IOCTL_WRITE_CONFIG, arg);
            if (ret < 0) {
                printk(KERN_ERR "Failed to write to BMP280_REG_CONFIG register\n");
                return ret;
            }
            break;
        default:
            printk(KERN_ERR "Invalid command\n");
            return -EINVAL;
    }
    return 0;
}

static int bmp280_open(struct inode *inodep, struct file *filep)
{
    int *data_buffer;
    printk(KERN_INFO "BMP280 device opened\n");
    // Bổ sung phần phân vùng bộ nhớ
    data_buffer = kmalloc(2 * sizeof(int), GFP_KERNEL);
    if (!data_buffer) {
        printk(KERN_ERR "Failed to allocate memory\n");
        return -ENOMEM; // Trả về lỗi nếu không thể cấp phát bộ nhớ
    }
    // Gán con trỏ dữ liệu vào trường private của filep để sử dụng sau này
    filep->private_data = data_buffer;
    return 0;
}

static int bmp280_release(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "BMP280 device closed\n");
    // Giải phóng vùng nhớ đã cấp phát trong hàm bmp280_open
    kfree(filep->private_data);
    return 0;
}

static struct file_operations fops = {
    .open = bmp280_open,
    .unlocked_ioctl = bmp280_ioctl,
    .release = bmp280_release,
};

static int bmp280_probe(struct i2c_client *client)//, const struct i2c_device_id *id)
{
    bmp280_client = client;
    /* Register major for BMP280 driver */
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ERR "Failed to register a major number\n");
        return major_number;
    }
    printk(KERN_INFO "BMP280: Registered successfully with major number %d\n", major_number);
    
    /* Register class for BMP280 driver */
    bmp280_class = class_create(CLASS_NAME);
    if (IS_ERR(bmp280_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "Failed to register device class\n");
        return PTR_ERR(bmp280_class);
    }
    printk(KERN_INFO "BMP280: Device class '%s' created successfully\n", CLASS_NAME);
    
    /* Register device for BMP280 driver */
    bmp280_device = device_create(bmp280_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(bmp280_device)) {
        class_destroy(bmp280_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "Failed to create the device\n");
        return PTR_ERR(bmp280_device);
    }
    printk(KERN_INFO "BMP280: Device '/dev/%s' created successfully\n", DEVICE_NAME);

    printk(KERN_INFO "BMP280 driver installed\n");
    return 0;
}

static void bmp280_remove(struct i2c_client *client)
{
    device_destroy(bmp280_class, MKDEV(major_number, 0));
    class_unregister(bmp280_class);
    class_destroy(bmp280_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "BMP280 driver removed\n");
}

static const struct of_device_id bmp280_of_match[] = {
    { .compatible = "invensense,bmp280", },
    { },
};
MODULE_DEVICE_TABLE(of, bmp280_of_match);

static struct i2c_driver bmp280_driver = {
    .driver = {
        .name   = DRIVER_NAME,
        .owner  = THIS_MODULE,
        .of_match_table = of_match_ptr(bmp280_of_match),
    },
    .probe      = bmp280_probe,
    .remove     = bmp280_remove,
};

static int __init bmp280_init(void)
{
    printk(KERN_INFO "Initializing BMP280 driver\n");
    return i2c_add_driver(&bmp280_driver); 
}

static void __exit bmp280_exit(void)
{
    printk(KERN_INFO "Exiting BMP280 driver\n");
    i2c_del_driver(&bmp280_driver);
}

module_init(bmp280_init);
module_exit(bmp280_exit);

MODULE_AUTHOR("KShelbyIOT-VTN");
MODULE_DESCRIPTION("BMP280 I2C Client Driver");
MODULE_LICENSE("GPL");
