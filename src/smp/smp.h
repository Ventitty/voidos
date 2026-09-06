#ifndef SMP_H
#define SMP_H

#include "src/types.h"
#include "arch/xtensa_lx6/xtensa.h"

void start_app_cpu(void);
void app_cpu_main(void);

#endif /* SMP_H */
