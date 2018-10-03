//
// Bareflank Hypervisor Trap SystemCall
// Copyright (C) 2018 morimolymoly (Tr4pMafia)
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

#include <bfvmm/hve/arch/intel_x64/vcpu/vcpu.h>
#include <bfvmm/vcpu/vcpu.h>
#include <bfdebug.h>
#include <bfvmm/vcpu/vcpu_factory.h>
#include <bfgsl.h>
#include <bfstring.h>
#include <string>
#include <vector>

namespace mafia
{
namespace intel_x64
{
static int pf_count = 0;

static bool trap_pf(gsl::not_null<bfvmm::intel_x64::vmcs *> vmcs) noexcept
{
    using namespace ::intel_x64::vmcs;

    vm_entry_interruption_information::vector::set(vm_exit_interruption_information::vector::get());
    vm_entry_interruption_information::interruption_type::set(vm_entry_interruption_information::interruption_type::hardware_exception);
    vm_entry_interruption_information::valid_bit::enable();
    vm_entry_interruption_information::deliver_error_code_bit::enable();
    vm_entry_exception_error_code::set(vm_exit_interruption_error_code::get());
    ::intel_x64::cr2::set(::intel_x64::cr2::get());


    // page fault orrcured when instruction fetch
    if ((vm_exit_interruption_error_code::get() & (1u << 4)) == (1u << 4)){
        ::intel_x64::cr2::set(vmcs->save_state()->rip);
    }

    if(pf_count < 2){
        // vmexit
        vm_exit_interruption_information::dump(0);
        vm_exit_interruption_error_code::dump(0);
        idt_vectoring_information::dump(0);
        idt_vectoring_error_code::dump(0);

        // vmentry
        vm_entry_interruption_information::dump(0);
        vm_entry_exception_error_code::dump(0);

        // cr2 and rip
        bfdebug_nhex(0, "cr2", cr2);
        bfdebug_nhex(0, "rip", vmcs->save_state()->rip);
        pf_count++;
    }

    return true;
}
class mafia_vcpu : public bfvmm::intel_x64::vcpu
{
public:
    mafia_vcpu(vcpuid::type id)
    : bfvmm::intel_x64::vcpu{id}
    {

        exit_handler()->add_handler(
            ::intel_x64::vmcs::exit_reason::basic_exit_reason::exception_or_non_maskable_interrupt,
            handler_delegate_t::create<mafia::intel_x64::trap_pf>());

        // trap page fault
        ::intel_x64::vmcs::exception_bitmap::set((1u << 14));
        ::intel_x64::vmcs::page_fault_error_code_mask::set(0);
        ::intel_x64::vmcs::page_fault_error_code_match::set(0);
    }
    ~mafia_vcpu() = default;
};
}
}

namespace bfvmm
{
std::unique_ptr<bfvmm::vcpu>
vcpu_factory::make_vcpu(vcpuid::type vcpuid, bfobject *obj)
{
    bfignored(obj);
    return std::make_unique<mafia::intel_x64::mafia_vcpu>(vcpuid);
}
}

