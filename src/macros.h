#ifndef VIMDAW_MACROS_H
#define VIMDAW_MACROS_H

#include <stdio.h>

// Define month detection macros fully:
#define MONTH_IS_JAN (__DATE__[0] == 'J' && __DATE__[1] == 'a')
#define MONTH_IS_FEB (__DATE__[0] == 'F')
#define MONTH_IS_MAR                                                           \
    (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'r')
#define MONTH_IS_APR (__DATE__[0] == 'A' && __DATE__[1] == 'p')
#define MONTH_IS_MAY                                                           \
    (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'y')
#define MONTH_IS_JUN                                                           \
    (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'n')
#define MONTH_IS_JUL                                                           \
    (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'l')
#define MONTH_IS_AUG (__DATE__[0] == 'A' && __DATE__[1] == 'u')
#define MONTH_IS_SEP (__DATE__[0] == 'S')
#define MONTH_IS_OCT (__DATE__[0] == 'O')
#define MONTH_IS_NOV (__DATE__[0] == 'N')
#define MONTH_IS_DEC (__DATE__[0] == 'D')

#define MONTH_NUMBER                                                           \
    (MONTH_IS_JAN ? 1 :                                                        \
     MONTH_IS_FEB ? 2 :                                                        \
     MONTH_IS_MAR ? 3 :                                                        \
     MONTH_IS_APR ? 4 :                                                        \
     MONTH_IS_MAY ? 5 :                                                        \
     MONTH_IS_JUN ? 6 :                                                        \
     MONTH_IS_JUL ? 7 :                                                        \
     MONTH_IS_AUG ? 8 :                                                        \
     MONTH_IS_SEP ? 9 :                                                        \
     MONTH_IS_OCT ? 10 :                                                       \
     MONTH_IS_NOV ? 11 :                                                       \
     MONTH_IS_DEC ? 12 :                                                       \
                    0)

#define DAY_CHAR1 (__DATE__[4] == ' ' ? '0' : __DATE__[4])
#define DAY_CHAR2 (__DATE__[5])

#define YEAR_CHAR1 (__DATE__[7])
#define YEAR_CHAR2 (__DATE__[8])
#define YEAR_CHAR3 (__DATE__[9])
#define YEAR_CHAR4 (__DATE__[10])

#ifdef DEBUG

#define PRINT_DATE_NUMERIC()                                                   \
    fprintf(stderr,                                                            \
            "%02d-%c%c-%c%c%c%c",                                              \
            MONTH_NUMBER,                                                      \
            DAY_CHAR1,                                                         \
            DAY_CHAR2,                                                         \
            YEAR_CHAR1,                                                        \
            YEAR_CHAR2,                                                        \
            YEAR_CHAR3,                                                        \
            YEAR_CHAR4)

#define CALL_CARMACK(fmt, ...)                                                 \
    do {                                                                       \
        fprintf(stderr, "\n" fmt, ##__VA_ARGS__);                              \
        if ((fmt)[sizeof(fmt) - 2] == '\n')                                    \
            fputc('\b', stderr); /* remove trailing newline visually */        \
        fprintf(stderr,                                                        \
                " [%s:%d:%s, %s, ",                                            \
                __FILE__,                                                      \
                __LINE__,                                                      \
                __func__,                                                      \
                __TIME__);                                                     \
        PRINT_DATE_NUMERIC();                                                  \
        fprintf(stderr, "]\n");                                                \
    } while (0)

#define header(fmt, ...)                                                       \
    do {                                                                       \
        fprintf(stderr, "\n" fmt, ##__VA_ARGS__);                              \
        if ((fmt)[sizeof(fmt) - 2] == '\n') fputc('\b', stderr);               \
        fprintf(stderr, " [%s:%d:%s]\n", __FILE__, __LINE__, __func__);        \
    } while (0)

#define end(fmt, ...) header(fmt, ##__VA_ARGS__)
#define END(fmt, ...) CALL_CARMACK(fmt, ##__VA_ARGS__)

// Add an extra newline after header for sub_header
#define sub_header(fmt, ...)                                                   \
    do {                                                                       \
        header(fmt, ##__VA_ARGS__);                                            \
        fprintf(stderr, "\n");                                                 \
    } while (0)

#define call_carmack(fmt, ...) fprintf(stderr, "- " fmt "\n", ##__VA_ARGS__)
#define nl() fprintf(stderr, "\n")

#else // DEBUG not defined

#define CALL_CARMACK(fmt, ...) ((void)0)
#define header(fmt, ...) ((void)0)
#define end(fmt, ...) ((void)0)
#define END(fmt, ...) ((void)0)
#define sub_header(fmt, ...) ((void)0)
#define call_carmack(fmt, ...) ((void)0)
#define nl() ((void)0)
#define PRINT_DATE_NUMERIC() ((void)0)

#endif // DEBUG

#endif // VIMDAW_MACROS_H
