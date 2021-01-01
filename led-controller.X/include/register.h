#ifndef REGISTER_H
#define	REGISTER_H

typedef volatile unsigned int atomic_reg_t;
typedef atomic_reg_t* atomic_reg_ptr_t;
typedef struct                  
{  
    atomic_reg_t reg;          
    atomic_reg_t clr;           
    atomic_reg_t set;             
    atomic_reg_t inv;       
} atomic_reg_group_t;

#define atomic_reg(name)                atomic_reg_group_t name
#define atomic_reg_value(reg)           atomic_reg_ptr_value(&reg)
#define atomic_reg_clr(reg, mask)       atomic_reg_ptr_clr(&reg, mask)
#define atomic_reg_set(reg, mask)       atomic_reg_ptr_set(&reg, mask)
#define atomic_reg_inv(reg, mask)       atomic_reg_ptr_inv(&reg, mask)

#define atomic_reg_ptr(name)            atomic_reg_group_t* name
#define atomic_reg_ptr_cast(addr)       ((atomic_reg_group_t*)addr)
#define atomic_reg_ptr_value(reg)       *(((atomic_reg_ptr_t)reg) + 0)
#define atomic_reg_ptr_clr(reg, mask)   *(((atomic_reg_ptr_t)reg) + 1) = mask
#define atomic_reg_ptr_set(reg, mask)   *(((atomic_reg_ptr_t)reg) + 2) = mask
#define atomic_reg_ptr_inv(reg, mask)   *(((atomic_reg_ptr_t)reg) + 3) = mask

#endif	/* REGISTER_H */