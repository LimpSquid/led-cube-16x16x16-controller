#ifndef PRINT_H
#define	PRINT_H

#include <stdarg.h>

int print_fs(char* str, const char* format, ...);
int print_vfs(char* str, const char* format, va_list arg);

#endif	/* PRINT_H */