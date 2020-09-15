#include <linux/sched.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/task.h>
#include <uapi/linux/sched/types.h>
#endif

enum revpi_power_led_mode {
	REVPI_POWER_LED_OFF = 0,
	REVPI_POWER_LED_ON = 1,
	REVPI_POWER_LED_FLICKR = 2,
	REVPI_POWER_LED_ON_500MS = 3,
	REVPI_POWER_LED_ON_1000MS = 4,
};

void revpi_led_trigger_event(u16 led_prev, u16 led);
void revpi_power_led_red_set(enum revpi_power_led_mode mode);
void revpi_power_led_red_run(void);

void revpi_check_timeout(void);

int bcm2835_cpufreq_clock_property(u32 tag, u32 id, u32 * val);
uint32_t bcm2835_cpufreq_get_clock(void);
extern char *lock_file;
extern int lock_line;

#if 0
#define my_rt_mutex_lock(P)	if (rt_mutex_is_locked(P)) { \
					ktime_t t0, t1; \
					s64 diff; \
					t0 = ktime_get(); \
					rt_mutex_lock(P); \
					t1 = ktime_get(); \
					diff = ktime_to_us(ktime_sub(t1, t0)); \
					pr_err("mutex already locked by %d %s\nnow: delay%5dus %d " __FILE__ "\n", lock_line, lock_file, (int)diff, __LINE__); \
				} else { \
					rt_mutex_lock(P); \
				} \
				lock_file = __FILE__; lock_line = __LINE__;

#else
#define my_rt_mutex_lock(P)	rt_mutex_lock(P)
#endif

struct kthread_prio {
	const char comm[TASK_COMM_LEN];
	int prio;
};

int set_kthread_prios(const struct kthread_prio *ktprios);
