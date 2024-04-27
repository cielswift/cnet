#ifndef _ASM_X86_PERF_REGS_H
#define _ASM_X86_PERF_REGS_H

enum perf_event_x86_regs {
	PERF_REG_X86_AX,
	PERF_REG_X86_BX,
	PERF_REG_X86_CX,
	PERF_REG_X86_DX,
	PERF_REG_X86_SI,
	PERF_REG_X86_DI,
	PERF_REG_X86_BP,
	PERF_REG_X86_SP,
	PERF_REG_X86_IP,
	PERF_REG_X86_FLAGS,
	PERF_REG_X86_CS,
	PERF_REG_X86_SS,
	PERF_REG_X86_DS,
	PERF_REG_X86_ES,
	PERF_REG_X86_FS,
	PERF_REG_X86_GS,
	PERF_REG_X86_R8,
	PERF_REG_X86_R9,
	PERF_REG_X86_R10,
	PERF_REG_X86_R11,
	PERF_REG_X86_R12,
	PERF_REG_X86_R13,
	PERF_REG_X86_R14,
	PERF_REG_X86_R15,
	/* These are the limits for the GPRs. */
	PERF_REG_X86_32_MAX = PERF_REG_X86_GS + 1,
	PERF_REG_X86_64_MAX = PERF_REG_X86_R15 + 1,

	/* These all need two bits set because they are 128bit */
	PERF_REG_X86_XMM0  = 32,
	PERF_REG_X86_XMM1  = 34,
	PERF_REG_X86_XMM2  = 36,
	PERF_REG_X86_XMM3  = 38,
	PERF_REG_X86_XMM4  = 40,
	PERF_REG_X86_XMM5  = 42,
	PERF_REG_X86_XMM6  = 44,
	PERF_REG_X86_XMM7  = 46,
	PERF_REG_X86_XMM8  = 48,
	PERF_REG_X86_XMM9  = 50,
	PERF_REG_X86_XMM10 = 52,
	PERF_REG_X86_XMM11 = 54,
	PERF_REG_X86_XMM12 = 56,
	PERF_REG_X86_XMM13 = 58,
	PERF_REG_X86_XMM14 = 60,
	PERF_REG_X86_XMM15 = 62,

	/* These include both GPRs and XMMX registers */
	PERF_REG_X86_XMM_MAX = PERF_REG_X86_XMM15 + 2,
};
#endif /* _ASM_X86_PERF_REGS_H */