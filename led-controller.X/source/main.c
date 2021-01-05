#include "../include/sys.h"
#include "../include/kernel.h"
#include "../include/kernel_task.h"
#include "../include/dma.h"
#include "../include/pwm.h"

int main()
{    
    // Bonzo is sleeping for the early init
    sys_goodnight_bonzo();
    sys_disable_global_interrupt();
    sys_cpu_early_init();
    
    // Initialize other stuff
    dma_init();
    pwm_init();
    
    kernel_init();
    sys_enable_global_interrupt();
    
    // Wakeup bonzo
    sys_wakeup_bonzo();
    
    // Bonzo is always hungry... 
    while(SYS_BONZO_IS_HUNGRY) {
        sys_feed_bonzo();
        kernel_execute();
    }
}