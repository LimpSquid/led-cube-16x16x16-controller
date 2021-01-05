#include "../include/print.h"
#include <stddef.h>
#include <string.h>

#define PRINT_CONVERT_STR_SIZE  50
#define PRINT_MAX_ZERO_PAD      31 // Make sure it fits in the pad_zero field

#define print_char_is_num(ch)   (ch >= '0' && ch <= '9')
#define print_char_to_val(ch)   (ch - '0')
#define print_next_char(ptr)    (*(++ptr))

struct print_options
{
    unsigned char upper_case    :1;
    unsigned char negative      :1;
    unsigned char alignment     :2; // Not implemented yet
    unsigned char pad_zero      :5; 
};

struct print_string
{
    char* buffer;
    int offset;
};

enum 
{
    ALIGN_LEFT = 0,
    ALIGN_RIGHT,
    ALIGN_CENTER
};

static void print_full_str(const char* str, int size, void* data, int(*puts)(void* data, const char* str, unsigned int size));
static int print_format(const char* format, va_list arg, void* data, int(*puts)(void* data, const char* str, unsigned int size));
static int print_strlen32(const char* str);
static int print_itoa(int value, char* str, int base, const struct print_options* opt);

static int print_str_puts(void* data, const char* str, unsigned int size);

int print_fs(char* str, const char* format, ...)
{
    int result = 0;
    if(NULL != str) {
        va_list arg;
        va_start(arg, format);
        struct print_string str_data = { .buffer = str, .offset = 0 };
        result = print_format(format, arg, &str_data, print_str_puts);
        va_end(arg);
    }
    return result;
}

int print_vfs(char* str, const char* format, va_list arg)
{
    int result = 0;
    if(NULL != str) {
        struct print_string str_data = { .buffer = str, .offset = 0 };
        result = print_format(format, arg, &str_data, print_str_puts);
    }
    return result;
}

static void print_full_str(const char* str, int size, void* data, int(*puts)(void* data, const char* str, unsigned int size))
{
    const char* start = str;
    const char* end = str + size;
    while(start != end)
        start += (*puts)(data, start, (end - start));
}

static int print_format(const char* format,  va_list arg, void* data, int(*puts)(void* data, const char* str, unsigned int size))
{
    struct print_options options = { 0 };
    int chars_written = 0;
    const char* marker = format;
    char str[PRINT_CONVERT_STR_SIZE];
    char current = *format;
    
    while(current != '\0') {
        if(current == '%') {
            // Write a part of the string
            print_full_str(marker, (format - marker), data, puts);
            chars_written += (format - marker);
            
            // Advance to next character
            current = print_next_char(format);
            
            // Zero padding
            if(current == '0') {
                unsigned int factor = 1;
                unsigned int padding = 0;
                current = print_next_char(format);
                while(current != '\0') {
                    if(print_char_is_num(current)) {
                        padding *= factor;
                        padding += print_char_to_val(current);
                        factor *= 10;
                        current = print_next_char(format);
                    } else
                        break;
                }
                options.pad_zero = (padding < PRINT_MAX_ZERO_PAD) ? padding : PRINT_MAX_ZERO_PAD;
            }
            
            switch(current) {
                // Unsupported fall through NULL case
                default:
                case '\0':
                    break;
                
                // Signed decimal
                case 'd':
                case 'i': {
                    options.negative = 1;
                    int size = print_itoa(va_arg(arg, int), str, 10, &options);
                    print_full_str(str, size, data, puts);
                    chars_written += size;
                    current = print_next_char(format);
                    break;
                }
                
                // Unsigned hexadecimal
                case 'x':
                case 'X': {
                    options.negative = 0;
                    options.upper_case = (current == 'X');
                    int size = print_itoa(va_arg(arg, int), str, 16, &options);
                    print_full_str(str, size, data, puts);
                    chars_written += size;
                    current = print_next_char(format);
                    break;
                }
                
                // String
                case 's': {
                    char* str = va_arg(arg, char*);
                    int size = print_strlen32(str);
                    print_full_str(str, size, data, puts);
                    chars_written += size;
                    current = print_next_char(format);
                    break;
                }
            }
            
            // Reset marker position
            marker = format;
        } else
            current = print_next_char(format);
    }
    
    print_full_str(marker, (format - marker), data, puts);
    chars_written += (format - marker);
    return chars_written;
}

static int print_strlen32(const char* str)
{
    int length = 0;
    while(1) {
        unsigned int x = *(unsigned int*)str;
        if((x & 0xff) == 0) 
            return length;
        if((x & 0xff00) == 0) 
            return length + 1;
        if((x & 0xff0000) == 0) 
            return length + 2;
        if((x & 0xff000000) == 0) 
            return length + 3;
        
        str += 4;
        length += 4;
    }
}

static int print_itoa(int value, char* str, int base, const struct print_options* opt)
{ 
    // Check boundaries of base, default to 10
    if(base < 2 || base > 16)
        base = 10;
    
    // Only base 10 can be negative
    unsigned char negative = (value < 0) && opt->negative && (base == 10);
    unsigned int pos_value = negative ? -value : value;
    unsigned int number = pos_value;
    unsigned int n_digits;
    
    // Determine how many digits the number has
    for(n_digits = 1; number /= base; n_digits++);
    
    unsigned char zero_padding = (opt->pad_zero >= (n_digits + negative)) ? opt->pad_zero - (n_digits + negative) : 0;
    int length = (zero_padding + n_digits + negative);
    char* string_builder = str + length;
    
    // Insert NULL terminator
    *(string_builder--) = '\0';
     
    // Build up the digits
    do {
		int digit = pos_value % base;
		*(string_builder--) = (digit < 10 ? '0' + digit : (opt->upper_case ? 'A' : 'a') + digit - 10);
		pos_value /= base;
    } while(pos_value > 0);
    
    // Zero padding
    while(zero_padding-- > 0)
        *(string_builder--) = '0';
    
    // Insert negative sign 
    if(negative)
        *(string_builder--) = '-';
    
    return length;
}

static int print_str_puts(void* data, const char* str, unsigned int size)
{
    struct print_string* str_data = data;
    memcpy(&str_data->buffer[str_data->offset], str, size);
    str_data->offset += size;
    return size;
}