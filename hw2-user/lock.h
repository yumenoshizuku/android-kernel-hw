#ifndef __LOCK_H_
#define __LOCK_H_

#include <sys/syscall.h>

#define __NR_netlock_acquire 378
#define __NR_netlock_release 379

enum __netlock_t {
	NET_LOCK_N, /* Placeholder for no lock */
	NET_LOCK_R, /* Indicates regular lock */
	NET_LOCK_E, /* Indicates exclusive lock */
};
typedef enum __netlock_t netlock_t;

inline int netlock_acquire(netlock_t type) {
    return syscall(__NR_netlock_acquire, type);
}
inline int netlock_release(void) {
	return syscall(__NR_netlock_release, NULL);
}
#endif  // __LOCK_H_
