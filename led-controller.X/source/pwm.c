#include "../include/pwm.h"
#include "../include/assert.h"
#include "../include/register.h"
#include "../include/sys.h"
#include "../include/toolbox.h"
#include <xc.h>
#include <stdbool.h>
#include <sys/attribs.h>


// Below are the output compare related defines
#define PWM_OC_PR(frequency)                (SYS_PB_CLOCK / (frequency * PWM_OC_PRESCALER) - 1)
#define PWM_OC_DUTY(frequency, percentage)  ((PWM_OC_PR(frequency) + 1) * percentage)
#define PWM_OC_PRESCALER                    1

#define PWM_OC_GSCLK_PPS                    RPE5R
#define PWM_OC_GSCLK_TRIS                   TRISE
#define PWM_OC_GSCLK_ANSEL                  ANSELE

#define PWM_OC_TCON_REG                     T2CON
#define PWM_OC_PR_REG                       PR2
#define PWM_OC_OCCON_REG                    OC4CON
#define PWM_OC_OCRS_REG                     OC4RS
#define PWM_OC_OCR_REG                      OC4R

#define PWM_OC_GSCLK_PPS_WORD               0xb
#define PWM_OC_TCON_WORD                    BIT(15)
#define PWM_OC_OCCON_WORD                   MASK(0x6, 0)

#define PWM_OC_GSCLK_PIN_MASK               BIT(5)
#define PWM_OC_OCCON_ON_MASK                BIT(15)

// Below are the timer related defines
#define PWM_TMR_PR(frequency, div)          ((SYS_PB_CLOCK * div) / (frequency * PWM_TMR_PRESCALER) - 1)
#define PWM_TMR_PRESCALER                   256

#define PWM_TMR_VECTOR                      _TIMER_3_VECTOR

#define PWM_TMR_TCON_REG                    T3CON
#define PWM_TMR_PR_REG                      PR3
#define PWM_TMR_TMR_REG                     TMR3
#define PWM_TMR_IEC_REG                     IEC0
#define PWM_TMR_IFS_REG                     IFS0
#define PWM_TMR_IPC_REG                     IPC3

#define PWM_TMR_TCON_WORD                   MASK(0x7, 4)

#define PWM_TMR_OCCON_ON_MASK               BIT(15)
#define PWM_TMR_INT_MASK                    BIT(14)
#define PWM_TMR_INT_PRIORITY_MASK           MASK(0x3, 2)

static void pwm_period_callback_dummy(void);

static void (*pwm_period_callback)(void) = &pwm_period_callback_dummy;

void pwm_init(void)
{
    // Configure PPS
    sys_unlock();
    PWM_OC_GSCLK_PPS = PWM_OC_GSCLK_PPS_WORD;
    sys_lock();
   
    // Configure IO
    REG_CLR(PWM_OC_GSCLK_ANSEL, PWM_OC_GSCLK_PIN_MASK);
    REG_SET(PWM_OC_GSCLK_TRIS, PWM_OC_GSCLK_PIN_MASK);
    
    // Configure interrupt
    REG_SET(PWM_TMR_IEC_REG, PWM_TMR_INT_MASK);
    REG_SET(PWM_TMR_IPC_REG, PWM_TMR_INT_PRIORITY_MASK);

    // Configure timers
    PWM_OC_TCON_REG = PWM_OC_TCON_WORD;
    PWM_TMR_TCON_REG = PWM_TMR_TCON_WORD;
    
    // Configure PWM module
    PWM_OC_OCCON_REG = PWM_OC_OCCON_WORD;
}

void pwm_configure(struct pwm_config config)
{
    ASSERT(config.duty >= 0.0 && config.duty <= 1.0);
    ASSERT(config.period_callback_div > 0);
    
    // Disable timer and PWM first
    REG_CLR(PWM_TMR_TCON_REG, PWM_TMR_OCCON_ON_MASK);
    REG_CLR(PWM_OC_OCCON_REG, PWM_OC_OCCON_ON_MASK);
    
    // Configure PWM
    PWM_OC_PR_REG = PWM_OC_PR(config.frequency);
    PWM_OC_OCRS_REG = PWM_OC_DUTY(config.frequency, config.duty);
    PWM_OC_OCR_REG = 0;
    
    // Configure timer
    PWM_TMR_PR_REG = PWM_TMR_PR(config.frequency, config.period_callback_div);
    pwm_period_callback = &pwm_period_callback_dummy;
    
    if(NULL != config.period_callback)
        pwm_period_callback = config.period_callback;
}

void pwm_enable(void)
{
    // Reset timer
    REG_CLR(PWM_TMR_IFS_REG, PWM_TMR_INT_MASK);
    PWM_TMR_TMR_REG = 0;
    
    // Enable timer and PWM
    REG_SET(PWM_TMR_TCON_REG, PWM_TMR_OCCON_ON_MASK);
    REG_SET(PWM_OC_OCCON_REG, PWM_OC_OCCON_ON_MASK);
}

void pwm_disable(void)
{
    // Disable timer and PWM
    REG_CLR(PWM_TMR_TCON_REG, PWM_TMR_OCCON_ON_MASK);
    REG_CLR(PWM_OC_OCCON_REG, PWM_OC_OCCON_ON_MASK);
}

static void pwm_period_callback_dummy(void)
{
    // Do nothing
}

void __ISR(PWM_TMR_VECTOR, IPL7AUTO) pwm_timer_interrupt(void)
{
    pwm_period_callback();
    REG_CLR(PWM_TMR_IFS_REG, PWM_TMR_INT_MASK);
}