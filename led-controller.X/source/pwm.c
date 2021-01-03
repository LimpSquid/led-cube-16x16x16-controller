#include "../include/pwm.h"
#include "../include/assert.h"
#include "../include/register.h"
#include "../include/sys.h"
#include <xc.h>
#include <stdbool.h>
#include <sys/attribs.h>

#define PWM_REG_SET(reg, mask)              (reg |= mask)
#define PWM_REG_CLR(reg, mask)              (reg &= ~mask)

// Below are the output compare related defines
#define PWM_OC_PR(frequency)                ((SYS_PB_CLOCK / (frequency * PWM_OC_PRESCALER)) - 1)
#define PWM_OC_DUTY(frequency, percentage)  ((PWM_OC_PR(frequency) + 1) * percentage)
#define PWM_OC_PRESCALER                    2

#define PWM_OC_GSCLK_PPS                    RPE5R
#define PWM_OC_GSCLK_TRIS                   TRISE
#define PWM_OC_GSCLK_ANSEL                  ANSELE

#define PWM_OC_TCON_REG                     T2CON
#define PWM_OC_PR_REG                       PR2
#define PWM_OC_OCCON_REG                    OC4CON
#define PWM_OC_OCRS_REG                     OC4RS
#define PWM_OC_OCR_REG                      OC4R

#define PWM_OC_GSCLK_PPS_WORD               0x0000000b
#define PWM_OC_TCON_WORD                    0x00008010
#define PWM_OC_PR_WORD                      0x00000000
#define PWM_OC_OCCON_WORD                   0x00000006
#define PWM_OC_OCRS_WORD                    0x00000000
#define PWM_OC_OCR_WORD                     0x00000000

#define PWM_OC_GSCLK_PIN_MASK               0x00000020
#define PWM_OC_OCCON_ON_MASK                0x00008000

// Below are the timer related defines
#define PWM_TMR_PR(frequency, cycle)        (((SYS_PB_CLOCK / (frequency * PWM_TMR_PRESCALER)) * cycle) - 1)
#define PWM_TMR_PRESCALER                   256

#define PWM_TMR_VECTOR                      _TIMER_3_VECTOR

#define PWM_TMR_TCON_REG                    T3CON
#define PWM_TMR_PR_REG                      PR3
#define PWM_TMR_TMR_REG                     TMR3
#define PWM_TMR_IEC_REG                     IEC0
#define PWM_TMR_IFS_REG                     IFS0

#define PWM_TMR_TCON_WORD                   0x00000070
#define PWM_TMR_PR_WORD                     0x00000000

#define PWM_TMR_OCCON_ON_MASK               0x00008000
#define PWM_TMR_INTERRUPT_MASK              0x00004000

static void pwm_cycle_callback_dummy(void);

static void (*pwm_cycle_callback)(void) = &pwm_cycle_callback_dummy;

void pwm_init(void)
{
    // Configure PPS
    sys_unlock();
    PWM_OC_GSCLK_PPS = PWM_OC_GSCLK_PPS_WORD;
    sys_lock();
   
    // Configure IO
    PWM_REG_CLR(PWM_OC_GSCLK_ANSEL, PWM_OC_GSCLK_PIN_MASK);
    PWM_REG_SET(PWM_OC_GSCLK_TRIS, PWM_OC_GSCLK_PIN_MASK);
    
    // Configure interrupt
    PWM_REG_SET(PWM_TMR_IEC_REG, PWM_TMR_INTERRUPT_MASK);

    // Configure timers
    PWM_OC_PR_REG = PWM_OC_PR_WORD;
    PWM_OC_TCON_REG = PWM_OC_TCON_WORD;
    PWM_TMR_PR_REG = PWM_TMR_PR_WORD;
    PWM_TMR_TCON_REG = PWM_TMR_TCON_WORD;
    
    // Configure PWM module
    PWM_OC_OCRS_REG = PWM_OC_OCRS_WORD;
    PWM_OC_OCR_REG = PWM_OC_OCR_WORD;
    PWM_OC_OCCON_REG = PWM_OC_OCCON_WORD;
}

void pwm_configure(struct pwm_config config)
{
    ASSERT(config.duty >= 0.0 && config.duty <= 1.0);
    ASSERT(config.cycle_count > 0);
    
    // Disable timer and PWM first
    PWM_REG_CLR(PWM_TMR_TCON_REG, PWM_TMR_OCCON_ON_MASK);
    PWM_REG_CLR(PWM_OC_OCCON_REG, PWM_OC_OCCON_ON_MASK);
    
    // Configure PWM
    PWM_OC_PR_REG = PWM_OC_PR(config.frequency);
    PWM_OC_OCRS_REG = PWM_OC_DUTY(config.frequency, config.duty);
    PWM_OC_OCR_REG = 0;
    
    // Configure timer
    PWM_TMR_PR_REG = PWM_TMR_PR(config.frequency, config.cycle_count);
    pwm_cycle_callback = &pwm_cycle_callback_dummy;
    
    if(NULL != config.cycle_callback)
        pwm_cycle_callback = config.cycle_callback;
}

void pwm_enable(void)
{
    // Reset timer
    PWM_REG_CLR(PWM_TMR_IFS_REG, PWM_TMR_INTERRUPT_MASK);
    PWM_TMR_TMR_REG = 0;
    
    // Enable timer and PWM
    PWM_REG_SET(PWM_TMR_TCON_REG, PWM_TMR_OCCON_ON_MASK);
    PWM_REG_SET(PWM_OC_OCCON_REG, PWM_OC_OCCON_ON_MASK);
}

void pwm_disable(void)
{
    // Disable timer and PWM
    PWM_REG_CLR(PWM_TMR_TCON_REG, PWM_TMR_OCCON_ON_MASK);
    PWM_REG_CLR(PWM_OC_OCCON_REG, PWM_OC_OCCON_ON_MASK);
}

void pwm_cycle_callback_dummy(void)
{
    // Does nothing
}

void __ISR(PWM_TMR_VECTOR, IPL7AUTO)pwm_timer_interrupt(void)
{
    pwm_cycle_callback();
    PWM_REG_CLR(PWM_TMR_IFS_REG, PWM_TMR_INTERRUPT_MASK);
}