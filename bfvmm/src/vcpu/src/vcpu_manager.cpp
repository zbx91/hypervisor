//
// Bareflank Hypervisor
//
// Copyright (C) 2015 Assured Information Security, Inc.
// Author: Rian Quinn        <quinnr@ainfosec.com>
// Author: Brendan Kerrigan  <kerriganb@ainfosec.com>
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

#include <debug.h>
#include <exception.h>
#include <vcpu/vcpu_manager.h>

// -----------------------------------------------------------------------------
// Mutex
// -----------------------------------------------------------------------------

#include <mutex>
std::mutex g_vcpu_manager_mutex;

// -----------------------------------------------------------------------------
// Implementation
// -----------------------------------------------------------------------------

vcpu_manager *
vcpu_manager::instance()
{
    static vcpu_manager self;
    return &self;
}

void
vcpu_manager::create_vcpu(uint64_t vcpuid)
{
    auto vcpu = m_vcpu_factory->make_vcpu(vcpuid);

    std::lock_guard<std::mutex> guard(g_vcpu_manager_mutex);
    m_vcpus[vcpuid] = vcpu;
}

void
vcpu_manager::delete_vcpu(uint64_t vcpuid)
{
    std::lock_guard<std::mutex> guard(g_vcpu_manager_mutex);
    auto iter = m_vcpus.find(vcpuid);

    if (iter == m_vcpus.end())
        throw std::invalid_argument("invalid vcpuid");

    auto hold_vcpu_until_deleted = iter->second;

    m_vcpus.erase(iter);
    g_vcpu_manager_mutex.unlock();
}

void
vcpu_manager::run_vcpu(uint64_t vcpuid)
{
    auto vcpu = get_vcpu(vcpuid);

    if (vcpu)
        vcpu->run();
    else
        throw std::invalid_argument("invalid vcpuid");

    bfdebug << "success: host os is " << bfcolor_green "now " << bfcolor_end
            << "in a vm on vcpuid = " << vcpuid << bfendl;
}

void
vcpu_manager::hlt_vcpu(uint64_t vcpuid)
{
    auto vcpu = get_vcpu(vcpuid);

    if (vcpu)
        vcpu->hlt();
    else
        throw std::invalid_argument("invalid vcpuid");

    bfdebug << "success: host os is " << bfcolor_red "not " << bfcolor_end
            << "in a vm on vcpuid = " << vcpuid << bfendl;
}

void
vcpu_manager::write(uint64_t vcpuid, const std::string &str) noexcept
{
    auto vcpu = get_vcpu(vcpuid);

    if (vcpu)
        vcpu->write(str);
}

vcpu_manager::vcpu_manager() :
    m_vcpu_factory(std::make_shared<vcpu_factory>())
{
}

std::shared_ptr<vcpu>
vcpu_manager::get_vcpu(uint64_t vcpuid) const
{
    std::lock_guard<std::mutex> guard(g_vcpu_manager_mutex);
    auto iter = m_vcpus.find(vcpuid);

    if (iter == m_vcpus.end())
        return nullptr;

    return iter->second;
}
