#ifndef PWM_H
#define	PWM_H

struct pwm_config
{
    void (*period_callback)(void);
    
    unsigned int period_callback_div;
    unsigned int frequency;
    float duty;
};

void pwm_init(void);
void pwm_configure(struct pwm_config config);
void pwm_enable(void);
void pwm_disable(void);

#endif	/* PWM_H */