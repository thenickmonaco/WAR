// WAR - make music with vim motions
// Copyright (C) 2025 Nick Monaco
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
// 
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

//=============================================================================
// src/debug_macros.h
//=============================================================================

#ifndef WAR_DEBUG_MACROS_H
#define WAR_DEBUG_MACROS_H

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

#if DEBUG

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
        fprintf(stderr, "# " fmt, ##__VA_ARGS__);                              \
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
        fprintf(stderr, "## " fmt, ##__VA_ARGS__);                             \
        if ((fmt)[sizeof(fmt) - 2] == '\n') fputc('\b', stderr);               \
        fprintf(stderr, " [%s:%d:%s]\n", __FILE__, __LINE__, __func__);        \
    } while (0)

#define dump_bytes(label, arr, len)                                            \
    do {                                                                       \
        fprintf(stderr, "- %-13s (%3zu bytes):", (label), (size_t)(len));      \
        for (size_t _i = 0; _i < (size_t)(len); ++_i)                          \
            fprintf(stderr, " %02X", (arr)[_i]);                               \
        fprintf(stderr, "\n");                                                 \
    } while (0)

#define end(fmt, ...) header("END " fmt, ##__VA_ARGS__)
#define END(fmt, ...) CALL_CARMACK("END " fmt, ##__VA_ARGS__)

#define call_carmack(fmt, ...) fprintf(stderr, "- " fmt "\n", ##__VA_ARGS__)

#else // DEBUG not defined

#define CALL_CARMACK(fmt, ...) ((void)0)
#define header(fmt, ...) ((void)0)
#define end(fmt, ...) ((void)0)
#define END(fmt, ...) ((void)0)
#define sub_header(fmt, ...) ((void)0)
#define call_carmack(fmt, ...) ((void)0)
#define PRINT_DATE_NUMERIC() ((void)0)
#define dump_bytes(label, arr, len) ((void)0)

#endif // DEBUG

#endif // WAR_MACROS_H
