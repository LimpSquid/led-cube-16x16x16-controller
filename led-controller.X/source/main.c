#include "../include/sys.h"
#include "../include/kernel.h"
#include "../include/kernel_task.h"
#include "../include/dma.h"

int main()
{    
    // Bonzo is sleeping for the early init
    sys_goodnight_bonzo();
    sys_cpu_early_init();
    
    // Initialize other stuff
    dma_init();
  
    // Wakeup bonzo for the kernel init
    sys_wakeup_bonzo();
    kernel_init();
    
    // Bonzo is always hungry... 
    while(SYS_BONZO_IS_HUNGRY) {
        sys_feed_bonzo();
        kernel_execute();
    }
}
