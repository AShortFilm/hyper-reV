#include "arch.h"

#ifdef _INTELMACHINE
#include <intrin.h>
#include <ia32-doc/ia32.hpp>
#endif

std::uint64_t arch::get_vmexit_reason()
{
#ifdef _INTELMACHINE
	std::uint64_t vmexit_reason = 0;

	__vmx_vmread(VMCS_EXIT_REASON, &vmexit_reason);

	return vmexit_reason;
#else
	return 0;
#endif
}

std::uint8_t arch::is_cpuid(std::uint64_t vmexit_reason)
{
#ifdef _INTELMACHINE
	return vmexit_reason == VMX_EXIT_REASON_EXECUTE_CPUID;
#else
	return 0;
#endif
}

std::uint8_t arch::is_slat_violation(std::uint64_t vmexit_reason)
{
#ifdef _INTELMACHINE
	return vmexit_reason == VMX_EXIT_REASON_EPT_VIOLATION;
#else
	return 0;
#endif
}

cr3 arch::get_guest_cr3()
{
	cr3 guest_cr3 = { };

#ifdef _INTELMACHINE
	__vmx_vmread(VMCS_GUEST_CR3, (std::uint64_t*)&guest_cr3);
#else

#endif

	return guest_cr3;
}

void arch::advance_rip()
{
#ifdef _INTELMACHINE
	std::uint64_t guest_rip = 0;
	std::uint64_t instruction_length = 0;

	__vmx_vmread(VMCS_GUEST_RIP, &guest_rip);
	__vmx_vmread(VMCS_VMEXIT_INSTRUCTION_LENGTH, &instruction_length);

	__vmx_vmwrite(VMCS_GUEST_RIP, guest_rip + instruction_length);
#else

#endif
}
