#include "slat.h"
#include "../memory_manager/memory_manager.h"
#include "../memory_manager/heap_manager.h"
#include "../crt/crt.h"
#include <ia32-doc/ia32.hpp>
#include <cstdint>

#ifdef _INTELMACHINE
#include <intrin.h>
#endif

namespace slat
{
	crt::mutex_t hook_mutex = { };
}

void slat::set_up()
{
	constexpr std::uint64_t hook_entries_wanted = 0x1000 / sizeof(hook_entry_t);

	void* hook_entries_allocation = heap_manager::allocate_page();

	available_hook_list_head = static_cast<hook_entry_t*>(hook_entries_allocation);

	hook_entry_t* current_entry = available_hook_list_head;

	for (std::uint64_t i = 0; i < hook_entries_wanted - 1; i++)
	{
		current_entry->set_next(current_entry + 1);
		current_entry->set_original_pfn(0);
		current_entry->set_shadow_pfn(0);

		current_entry = current_entry->get_next();
	}

	current_entry->set_original_pfn(0);
	current_entry->set_shadow_pfn(0);
	current_entry->set_next(nullptr);
}

cr3 slat::get_cr3()
{
	cr3 slat_cr3 = { };

#ifdef _INTELMACHINE
	ept_pointer ept_pointer = { };

	__vmx_vmread(VMCS_CTRL_EPT_POINTER, &ept_pointer.flags);

	slat_cr3.address_of_page_directory = ept_pointer.page_frame_number;
#else

#endif

	return slat_cr3;
}

ept_pml4e* slat_get_pml4e(cr3 slat_cr3, virtual_address_t guest_physical_address)
{
	ept_pml4e* ept_pml4 = reinterpret_cast<ept_pml4e*>(memory_manager::map_host_physical(slat_cr3.address_of_page_directory << 12));

	ept_pml4e* pml4e = &ept_pml4[guest_physical_address.pml4_idx];

	return pml4e;
}

ept_pdpte* slat_get_pdpte(ept_pml4e* pml4e, virtual_address_t guest_physical_address)
{
	ept_pdpte* ept_pdpt = reinterpret_cast<ept_pdpte*>(memory_manager::map_host_physical(pml4e->page_frame_number << 12));

	ept_pdpte* pdpte = &ept_pdpt[guest_physical_address.pdpt_idx];

	return pdpte;
}

ept_pde* slat_get_pde(ept_pdpte* pdpte, virtual_address_t guest_physical_address)
{
	ept_pde* ept_pd = reinterpret_cast<ept_pde*>(memory_manager::map_host_physical(pdpte->page_frame_number << 12));

	ept_pde* pde = &ept_pd[guest_physical_address.pd_idx];

	return pde;
}

ept_pte* slat_get_pte(ept_pde* pde, virtual_address_t guest_physical_address)
{
	ept_pte* ept_pt = reinterpret_cast<ept_pte*>(memory_manager::map_host_physical(pde->page_frame_number << 12));

	ept_pte* pte = &ept_pt[guest_physical_address.pt_idx];

	return pte;
}

std::uint64_t slat::translate_guest_physical_address(cr3 slat_cr3, virtual_address_t guest_physical_address, std::uint64_t* size_left_of_slat_page)
{
#ifdef _INTELMACHINE
	ept_pml4e* pml4e = slat_get_pml4e(slat_cr3, guest_physical_address);

	ept_pdpte* pdpte = slat_get_pdpte(pml4e, guest_physical_address);

	ept_pdpte_1gb large_pdpte = { .flags = pdpte->flags };

	if (large_pdpte.large_page == 1)
	{
		const std::uint64_t page_offset = (guest_physical_address.pd_idx << 21) + (guest_physical_address.pt_idx << 12) + guest_physical_address.offset;

		if (size_left_of_slat_page != nullptr)
		{
			*size_left_of_slat_page = (1ull << 30) - page_offset;
		}

		return (large_pdpte.page_frame_number << 30) + page_offset;
	}

	ept_pde* pde = slat_get_pde(pdpte, guest_physical_address);

	ept_pde_2mb large_pde = { .flags = pde->flags };

	if (large_pde.large_page == 1)
	{
		const std::uint64_t page_offset = (guest_physical_address.pt_idx << 12) + guest_physical_address.offset;

		if (size_left_of_slat_page != nullptr)
		{
			*size_left_of_slat_page = (1ull << 21) - page_offset;
		}

		return (large_pde.page_frame_number << 21) + page_offset;
	}

	ept_pte* pte = slat_get_pte(pde, guest_physical_address);

	const std::uint64_t page_offset = guest_physical_address.offset;

	if (size_left_of_slat_page != nullptr)
	{
		*size_left_of_slat_page = (1ull << 12) - page_offset;
	}

	return (pte->page_frame_number << 12) + page_offset;
#else
	return 0;
#endif
}

std::uint8_t split_2mb_slat_pde(ept_pde_2mb* large_pde)
{
	ept_pte* pt = static_cast<ept_pte*>(heap_manager::allocate_page());

	if (pt == nullptr)
	{
		return 0;
	}

	for (std::uint64_t i = 0; i < 512; i++)
	{
		ept_pte* pte = &pt[i];

		pte->flags = 0;

		pte->execute_access = large_pde->execute_access;
		pte->read_access = large_pde->read_access;
		pte->write_access = large_pde->write_access;
		pte->memory_type = large_pde->memory_type;
		pte->ignore_pat = large_pde->ignore_pat;
		pte->user_mode_execute = large_pde->user_mode_execute;
		pte->verify_guest_paging = large_pde->verify_guest_paging;
		pte->paging_write_access = large_pde->paging_write_access;
		pte->supervisor_shadow_stack = large_pde->supervisor_shadow_stack;
		pte->suppress_ve = large_pde->suppress_ve;

		pte->page_frame_number = (large_pde->page_frame_number << 9) + i;
	}

	std::uint64_t pt_physical_address = memory_manager::unmap_host_physical(reinterpret_cast<std::uint64_t>(pt));

	ept_pde new_pde = { .flags = 0 };

	new_pde.page_frame_number = pt_physical_address >> 12;
	new_pde.read_access = 1;
	new_pde.write_access = 1;
	new_pde.execute_access = 1;
	new_pde.user_mode_execute = 1;

	large_pde->flags = new_pde.flags;

	return 1;
}

ept_pte* slat_get_pte(cr3 slat_cr3, virtual_address_t guest_physical_address, std::uint8_t force_split_pages = 0)
{
	ept_pml4e* pml4e = slat_get_pml4e(slat_cr3, guest_physical_address);

	ept_pdpte* pdpte = slat_get_pdpte(pml4e, guest_physical_address);

	ept_pdpte_1gb large_pdpte = { .flags = pdpte->flags };

	if (large_pdpte.large_page == 1)
	{
		return nullptr;
	}

	ept_pde* pde = slat_get_pde(pdpte, guest_physical_address);

	ept_pde_2mb* large_pde = reinterpret_cast<ept_pde_2mb*>(pde);

	if (large_pde->large_page == 1)
	{
		if (force_split_pages == 0 || split_2mb_slat_pde(large_pde) == 0)
		{
			return nullptr;
		}
	}

	return slat_get_pte(pde, guest_physical_address);;
}

std::uint64_t slat::add_slat_code_hook(cr3 slat_cr3, virtual_address_t target_guest_physical_address, virtual_address_t shadow_page_guest_physical_address)
{
#ifdef _INTELMACHINE
	hook_mutex.lock();

	hook_entry_t* already_present_hook_entry = hook_entry_t::find(used_hook_list_head, target_guest_physical_address.address >> 12);

	if (already_present_hook_entry != nullptr)
	{
		hook_mutex.release();

		return 0;
	}

	ept_pte* target_pte = slat_get_pte(slat_cr3, target_guest_physical_address, 1);

	if (target_pte == nullptr)
	{
		hook_mutex.release();

		return 0;
	}

	std::uint64_t shadow_page_host_physical_address = translate_guest_physical_address(slat_cr3, shadow_page_guest_physical_address);

	if (shadow_page_host_physical_address == 0)
	{
		hook_mutex.release();

		return 0;
	}

	hook_entry_t* hook_entry = available_hook_list_head;

	if (hook_entry == nullptr)
	{
		hook_mutex.release();

		return 0;
	}

	available_hook_list_head = hook_entry->get_next();

	hook_entry->set_next(used_hook_list_head);
	hook_entry->set_original_pfn(target_pte->page_frame_number);
	hook_entry->set_shadow_pfn(shadow_page_host_physical_address >> 12);

	used_hook_list_head = hook_entry;

	target_pte->execute_access = 0;

	hook_mutex.release();

	return 1;
#else
	return 0;
#endif
}

std::uint8_t slat::process_slat_violation()
{
#ifdef _INTELMACHINE
	vmx_exit_qualification_ept_violation qualification = { };

	__vmx_vmread(VMCS_EXIT_QUALIFICATION, &qualification.flags);

	if (qualification.caused_by_translation == 0)
	{
		return 0;
	}

	std::uint64_t physical_address = 0;

	__vmx_vmread(VMCS_GUEST_PHYSICAL_ADDRESS, &physical_address);

	hook_entry_t* hook_entry = hook_entry_t::find(used_hook_list_head, physical_address >> 12);

	if (hook_entry == nullptr)
	{
		return 0;
	}

	ept_pte* pte = slat_get_pte(get_cr3(), { .address = physical_address });

	if (pte == nullptr)
	{
		return 0;
	}

	if (qualification.execute_access == 1)
	{
		pte->read_access = 0;
		pte->write_access = 0;
		pte->execute_access = 1;

		pte->page_frame_number = hook_entry->get_shadow_pfn();
	}
	else
	{
		pte->read_access = 1;
		pte->write_access = 1;
		pte->execute_access = 0;

		pte->page_frame_number = hook_entry->get_original_pfn();
	}

	return 1;
#else
	return 0;
#endif
}

slat::hook_entry_t* slat::hook_entry_t::get_next() const
{
	return reinterpret_cast<hook_entry_t*>(this->next);
}

void slat::hook_entry_t::set_next(hook_entry_t* next_entry)
{
	this->next = reinterpret_cast<std::uint64_t>(next_entry);
}

std::uint64_t slat::hook_entry_t::get_original_pfn() const
{
	return (this->higher_original_pfn << 32) | this->lower_original_pfn;
}

std::uint64_t slat::hook_entry_t::get_shadow_pfn() const
{
	return (this->higher_shadow_pfn << 32) | this->lower_shadow_pfn;
}

void slat::hook_entry_t::set_original_pfn(const std::uint64_t original_pfn)
{
	this->lower_original_pfn = static_cast<std::uint32_t>(original_pfn);
	this->higher_original_pfn = original_pfn >> 32;
}

void slat::hook_entry_t::set_shadow_pfn(const std::uint64_t shadow_pfn)
{
	this->lower_shadow_pfn = static_cast<std::uint32_t>(shadow_pfn);
	this->higher_shadow_pfn = shadow_pfn >> 32;
}

slat::hook_entry_t* slat::hook_entry_t::find(hook_entry_t* list_head, std::uint64_t target_original_pfn)
{
	hook_entry_t* current_entry = list_head;

	while (current_entry != nullptr)
	{
		if (current_entry->get_original_pfn() == target_original_pfn)
		{
			return current_entry;
		}

		current_entry = current_entry->get_next();
	}

	return nullptr;
}
