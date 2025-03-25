#include "slat.h"
#include "../memory_manager/memory_manager.h"
#include <ia32-doc/ia32.hpp>
#include <cstdint>

#ifdef _INTELMACHINE
#include <intrin.h>
#endif

cr3 slat::get_cr3()
{
	cr3 slat_cr3 = { };

#ifdef _INTELMACHINE
	ept_pointer ept_pointer = { };

	__vmx_vmread(VMCS_CTRL_EPT_POINTER, (std::uint64_t*)&ept_pointer);

	slat_cr3.address_of_page_directory = ept_pointer.page_frame_number;
#else

#endif

	return slat_cr3;
}

std::uint64_t slat::translate_guest_physical_address(cr3 slat_cr3, virtual_address_t guest_physical_address)
{
#ifdef _INTELMACHINE
	ept_pml4e* ept_pml4 = reinterpret_cast<ept_pml4e*>(memory_manager::map_host_physical(slat_cr3.address_of_page_directory << 12));

	ept_pml4e pml4e = ept_pml4[guest_physical_address.pml4_idx];

	ept_pdpte* ept_pdpt = reinterpret_cast<ept_pdpte*>(memory_manager::map_host_physical(pml4e.page_frame_number << 12));

	ept_pdpte pdpte = ept_pdpt[guest_physical_address.pdpt_idx];

	ept_pdpte_1gb large_pdpte = { 0 };

	large_pdpte.flags = pdpte.flags;

	if (large_pdpte.large_page == 1)
	{
		const std::uint64_t page_offset = (guest_physical_address.pd_idx << 21) + (guest_physical_address.pt_idx << 12) + guest_physical_address.offset;

		return (large_pdpte.page_frame_number << 30) + page_offset;
	}

	ept_pde* ept_pd = reinterpret_cast<ept_pde*>(memory_manager::map_host_physical(pdpte.page_frame_number << 12));

	ept_pde pde = ept_pd[guest_physical_address.pd_idx];

	ept_pde_2mb large_pde = { 0 };

	large_pde.flags = pde.flags;

	if (large_pde.large_page == 1)
	{
		const std::uint64_t page_offset = (guest_physical_address.pt_idx << 12) + guest_physical_address.offset;

		return (large_pde.page_frame_number << 21) + page_offset;
	}

	ept_pte* ept_pt = reinterpret_cast<ept_pte*>(memory_manager::map_host_physical(pde.page_frame_number << 12));

	ept_pte pt = ept_pt[guest_physical_address.pt_idx];

	const std::uint64_t page_offset = guest_physical_address.offset;

	return (pt.page_frame_number << 12) + page_offset;
#else
	return 0;
#endif
}
