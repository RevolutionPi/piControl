#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
#define ktime_equal(a, b) ((a) == (b))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
#define BCM2835_THERMAL_ZONE "bcm2835_thermal"
#else
#define BCM2835_THERMAL_ZONE "cpu-thermal"
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
#define kernel_read(file, buf, count, pos)				\
	({								\
		ssize_t ret = kernel_read(file, *(pos), buf, count);	\
		if (ret > 0)						\
			*(pos) += ret;					\
		ret;							\
	})
#define kernel_write(file, buf, count, pos)				\
	({								\
		ssize_t ret = kernel_write(file, buf, count, *(pos));	\
		if (ret > 0)						\
			*(pos) += ret;					\
		ret;							\
	})
#endif
