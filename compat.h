#ifndef _COMPAT_H
#define _COMPAT_H
#include <linux/version.h>

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
#endif /* _COMPAT_H */
