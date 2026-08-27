/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
* Copyright (C) 2020 Loongson Technology Corporation Limited
*
* Author: Hanlu Li <lihanlu@loongson.cn>
*/
#ifndef _ASM_SIGINFO_H
#define _ASM_SIGINFO_H

#if _LOONGARCH_SZLONG == 32
#define __ARCH_SI_PREAMBLE_SIZE (3 * sizeof(int))
#else
#define __ARCH_SI_PREAMBLE_SIZE (4 * sizeof(int))
#endif

#include <asm-generic/siginfo.h>

#endif /* _ASM_SIGINFO_H */
