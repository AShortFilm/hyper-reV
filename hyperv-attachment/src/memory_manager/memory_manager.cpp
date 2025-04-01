#include "memory_manager.h"
#include "../slat/slat.h"

constexpr std::uint64_t host_physical_memory_access_base = 255ull << 39;

std::uint64_t memory_manager::map_host_physical(std::uint64_t host_physical_address)
{
	return host_physical_memory_access_base + host_physical_address;
}

std::uint64_t memory_manager::unmap_host_physical(std::uint64_t host_mapped_address)
{
	return host_mapped_address - host_physical_memory_access_base;
}

std::uint64_t memory_manager::map_guest_physical(cr3 slat_cr3, std::uint64_t guest_physical_address, std::uint64_t* size_left_of_ept_page)
{
	virtual_address_t guest_physical_address_to_map = { .address = guest_physical_address };

	const std::uint64_t host_physical_address = slat::translate_guest_physical_address(slat_cr3, guest_physical_address_to_map, size_left_of_ept_page);

	return host_physical_memory_access_base + host_physical_address;
}

std::uint64_t memory_manager::translate_guest_virtual_address(cr3 guest_cr3, cr3 slat_cr3, virtual_address_t guest_virtual_address, std::uint64_t* size_left_of_page)
{
	pml4e_64* pml4 = reinterpret_cast<pml4e_64*>(memory_manager::map_guest_physical(slat_cr3, guest_cr3.address_of_page_directory << 12));

	pml4e_64 pml4e = pml4[guest_virtual_address.pml4_idx];

	if (pml4e.present == 0)
	{
		return 0;
	}

	pdpte_64* pdpt = reinterpret_cast<pdpte_64*>(map_guest_physical(slat_cr3, pml4e.page_frame_number << 12));

	pdpte_64 pdpte = pdpt[guest_virtual_address.pdpt_idx];

	if (pdpte.present == 0)
	{
		return 0;
	}

	if (pdpte.large_page == 1)
	{
		pdpte_1gb_64 large_pdpte = { .flags = pdpte.flags };

		const std::uint64_t page_offset = (guest_virtual_address.pd_idx << 21) + (guest_virtual_address.pt_idx << 12) + guest_virtual_address.offset;

		if (size_left_of_page != nullptr)
		{
			*size_left_of_page = (1ull << 30) - page_offset;
		}

		return (large_pdpte.page_frame_number << 30) + page_offset;
	}

	pde_64* pd = reinterpret_cast<pde_64*>(map_guest_physical(slat_cr3, pdpte.page_frame_number << 12));

	pde_64 pde = pd[guest_virtual_address.pd_idx];

	if (pde.present == 0)
	{
		return 0;
	}

	if (pde.large_page == 1)
	{
		pde_2mb_64 large_pde = { .flags = pde.flags };

		const std::uint64_t page_offset = (guest_virtual_address.pt_idx << 12) + guest_virtual_address.offset;

		if (size_left_of_page != nullptr)
		{
			*size_left_of_page = (1ull << 21) - page_offset;
		}

		return (large_pde.page_frame_number << 21) + page_offset;
	}

	pte_64* pt = reinterpret_cast<pte_64*>(map_guest_physical(slat_cr3, pde.page_frame_number << 12));

	pte_64 pte = pt[guest_virtual_address.pt_idx];

	if (pte.present == 0)
	{
		return 0;
	}

	const std::uint64_t page_offset = guest_virtual_address.offset;

	if (size_left_of_page != nullptr)
	{
		*size_left_of_page = (1ull << 12) - page_offset;
	}

	return (pte.page_frame_number << 12) + page_offset;
}
