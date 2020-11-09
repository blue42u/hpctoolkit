#ifndef ITIMER_H
#define ITIMER_H

#include <stdbool.h>
#include <hpcrun/ompt/ompt-defer.h>

__attribute__((visibility("default")))
extern void hpcrun_itimer_wallclock_ok(bool flag);

#endif // ITIMER_H
