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

#include <exception.h>
#include <ioctl_private.h>
#include <commit_or_rollback.h>
#include <driver_entry_interface.h>

#include <SetupAPI.h>

// -----------------------------------------------------------------------------
// Unit Test Seems
// -----------------------------------------------------------------------------

HANDLE
bf_ioctl_open()
{
    HANDLE hDevInfo;
    SP_INTERFACE_DEVICE_DETAIL_DATA *deviceDetailData = nullptr;

    SP_DEVINFO_DATA devInfo;
    devInfo.cbSize = sizeof(SP_DEVINFO_DATA);

    SP_INTERFACE_DEVICE_DATA ifInfo;
    ifInfo.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);

    hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_bareflank, 0, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    if (hDevInfo == INVALID_HANDLE_VALUE)
        return hDevInfo;

    if (SetupDiEnumDeviceInfo(hDevInfo, 0, &devInfo) == false)
        return INVALID_HANDLE_VALUE;

    if (SetupDiEnumDeviceInterfaces(hDevInfo, &devInfo, &(GUID_DEVINTERFACE_bareflank), 0, &ifInfo) == false)
        return INVALID_HANDLE_VALUE;

    DWORD requiredSize = 0;

    if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &ifInfo, NULL, 0, &requiredSize, NULL) == true)
        return INVALID_HANDLE_VALUE;

    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        return INVALID_HANDLE_VALUE;

    deviceDetailData = (SP_INTERFACE_DEVICE_DETAIL_DATA *)malloc(requiredSize);

    if (deviceDetailData == nullptr)
        return INVALID_HANDLE_VALUE;

    auto cor1 = commit_or_rollback([&]
    { free(deviceDetailData); });

    deviceDetailData->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);

    if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &ifInfo, deviceDetailData, requiredSize, NULL, NULL) == false)
        return INVALID_HANDLE_VALUE;

    return CreateFile(deviceDetailData->DevicePath,
                      GENERIC_READ | GENERIC_WRITE,
                      0,
                      NULL,
                      CREATE_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL,
                      NULL);
}

int64_t
bf_send_ioctl(HANDLE fd, unsigned long request)
{
    if (!DeviceIoControl(fd, request, NULL, 0, NULL, 0, NULL, NULL))
        return BF_IOCTL_FAILURE;

    return 0;
}

int64_t
bf_read_ioctl(HANDLE fd, unsigned long request, void *data, size_t size)
{
    if (!DeviceIoControl(fd, request, NULL, 0, data, size, NULL, NULL))
        return BF_IOCTL_FAILURE;

    return 0;
}

int64_t
bf_write_ioctl(HANDLE fd, unsigned long request, const void *data, size_t size)
{
    if (!DeviceIoControl(fd, request, (LPVOID)data, size, NULL, 0, NULL, NULL))
        return BF_IOCTL_FAILURE;

    return 0;
}

// -----------------------------------------------------------------------------
// Implementation
// -----------------------------------------------------------------------------

ioctl_private::ioctl_private() :
    fd(INVALID_HANDLE_VALUE)
{
}

ioctl_private::~ioctl_private()
{
    if (fd != INVALID_HANDLE_VALUE)
        CloseHandle(fd);
}

void
ioctl_private::open()
{
    if ((fd = bf_ioctl_open()) == INVALID_HANDLE_VALUE)
        throw driver_inaccessible();
}

void
ioctl_private::call_ioctl_add_module(const char *data, int64_t len)
{
    if (data == 0)
        throw std::invalid_argument("data == NULL");

    if (len == 0)
        throw std::invalid_argument("len == 0");

    if (bf_write_ioctl(fd, IOCTL_ADD_MODULE, data, len) < 0)
        throw ioctl_failed(IOCTL_ADD_MODULE);
}

void
ioctl_private::call_ioctl_load_vmm()
{
    if (bf_send_ioctl(fd, IOCTL_LOAD_VMM) < 0)
        throw ioctl_failed(IOCTL_LOAD_VMM);
}

void
ioctl_private::call_ioctl_unload_vmm()
{
    if (bf_send_ioctl(fd, IOCTL_UNLOAD_VMM) < 0)
        throw ioctl_failed(IOCTL_UNLOAD_VMM);
}

void
ioctl_private::call_ioctl_start_vmm()
{
    if (bf_send_ioctl(fd, IOCTL_START_VMM) < 0)
        throw ioctl_failed(IOCTL_START_VMM);
}

void
ioctl_private::call_ioctl_stop_vmm()
{
    if (bf_send_ioctl(fd, IOCTL_STOP_VMM) < 0)
        throw ioctl_failed(IOCTL_STOP_VMM);
}

void
ioctl_private::call_ioctl_dump_vmm(debug_ring_resources_t *drr, uint64_t vcpuid)
{
    if (drr == 0)
        throw std::invalid_argument("drr == NULL");

    if (bf_write_ioctl(fd, IOCTL_SET_VCPUID, &vcpuid, sizeof(vcpuid)) < 0)
        throw ioctl_failed(IOCTL_SET_VCPUID);

    if (bf_read_ioctl(fd, IOCTL_DUMP_VMM, drr, sizeof(*drr)) < 0)
        throw ioctl_failed(IOCTL_DUMP_VMM);
}

void
ioctl_private::call_ioctl_vmm_status(int64_t *status)
{
    if (status == 0)
        throw std::invalid_argument("status == NULL");

    if (bf_read_ioctl(fd, IOCTL_VMM_STATUS, status, sizeof(*status)) < 0)
        throw ioctl_failed(IOCTL_VMM_STATUS);
}
