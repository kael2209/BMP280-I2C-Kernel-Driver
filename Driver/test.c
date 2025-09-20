/* Author: K.Shelby IOT - VTN */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h> 
#include <math.h>

#define DEVICE_PATH "/dev/bmp280"
#define BMP280_IOCTL_MAGIC 'm'
#define BMP280_IOCTL_READ_TEMPERATURE _IOR(BMP280_IOCTL_MAGIC, 1, int)
#define BMP280_IOCTL_READ_PRESSURE _IOR(BMP280_IOCTL_MAGIC, 2, int)
#define BMP280_IOCTL_WRITE_CONTROL _IOR(BMP280_IOCTL_MAGIC, 3, int)
#define BMP280_IOCTL_WRITE_CONFIG _IOR(BMP280_IOCTL_MAGIC, 4, int)

float calculate_altitude(float pressure) {
    float sea_level_pressure = 1013.25; 
    float altitude;
    altitude = 44330 * (1 - pow((pressure / sea_level_pressure), 0.1903));
    return altitude;
}

int main() {
    int fd, temperature, pressure;
    float altitude;
    /* Open the device */ 
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device");
        return errno;
    }
    if (ioctl(fd, BMP280_IOCTL_WRITE_CONTROL, 0x3F) < 0) {
        perror("Failed to write control register");
        close(fd);
        return errno;
    }
    if (ioctl(fd, BMP280_IOCTL_WRITE_CONFIG, 0xA0) < 0) {
        perror("Failed to write config register");
        close(fd);
        return errno;
    }
    while(1)
    {
        /* Read temperature */
        if (ioctl(fd, BMP280_IOCTL_READ_TEMPERATURE, &temperature) < 0) {
            perror("Failed to read temperature");
            close(fd);
            return errno;
        }
        printf("Temperature: %.2f °C     ", (float)temperature / 100.0);

        /* Read pressure */
        if (ioctl(fd, BMP280_IOCTL_READ_PRESSURE, &pressure) < 0) {
            perror("Failed to read pressure");
            close(fd);
            return errno;
        }
        printf("Pressure: %.2f hPa     ", (float)pressure / 100.0);

        /* Calculate altitude */
        altitude = calculate_altitude((float)pressure / 100.0);
        printf("Altitude: %.2f meters\n", altitude);

        sleep(1);
    }
    close(fd);
    return 0;
}

