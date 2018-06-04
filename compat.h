#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
#define ktime_equal(a, b) ((a) == (b))
#endif
