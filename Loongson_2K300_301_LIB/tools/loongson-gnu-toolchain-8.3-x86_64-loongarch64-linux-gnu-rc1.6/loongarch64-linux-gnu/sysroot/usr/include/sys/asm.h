#ifndef _SYS_ASM_H
#define _SYS_ASM_H

#include <sys/regdef.h>
#include <sysdeps/generic/sysdep.h>

/* Macros to handle different pointer/register sizes for 32/64-bit code.  */
#ifdef __loongarch64
# define PTRLOG 3
# define SZREG	8
# define SZFREG	8
# define SZVREG 16
# define SZXREG 32
# define REG_L ld.d
# define REG_S st.d
# define SRLI srli.d
# define SLLI slli.d
# define ADDI addi.d
# define ADD  add.d
# define SUB  sub.d
# define BSTRINS  bstrins.d
# define LI  li.d
# define FREG_L fld.d
# define FREG_S fst.d
#elif defined __loongarch32
# define PTRLOG 2
# define SZREG	4
# define SZFREG	4
# define REG_L ld.w
# define REG_S st.w
# define FREG_L fld.w
# define FREG_S fst.w
#else
# error __loongarch_xlen must equal 32 or 64
#endif


/*  Declare leaf routine.
    The usage of macro LEAF/ENTRY is as follows:
    1. LEAF(fcn) -- the align value of fcn is .align 3 (default value)
    2. LEAF(fcn, 6) -- the align value of fcn is .align 6
*/
#define	LEAF_IMPL(symbol, aln, ...)	\
	.text;				\
	.globl	symbol;			\
	.align	aln;			\
	.type	symbol, @function;	\
symbol: \
	cfi_startproc;

#define LEAF(...) LEAF_IMPL(__VA_ARGS__, 3)
#define ENTRY(...) LEAF(__VA_ARGS__)

#define	LEAF_NO_ALIGN(symbol)			\
	.text;				\
	.globl	symbol;			\
	.type	symbol, @function;	\
symbol: \
	cfi_startproc;

#define ENTRY_NO_ALIGN(symbol) LEAF_NO_ALIGN(symbol)

/* Mark end of function.  */
#undef END
#define END(function)			\
	cfi_endproc ;			\
	.size	function,.-function;

/* Stack alignment.  */
#define ALMASK	~15

#endif /* sys/asm.h */
