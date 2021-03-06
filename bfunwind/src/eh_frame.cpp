//
// Bareflank Unwind Library
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

#include <log.h>
#include <abort.h>
#include <dwarf4.h>
#include <eh_frame.h>

// -----------------------------------------------------------------------------
// Global
// -----------------------------------------------------------------------------

extern uint64_t g_phase;

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

uint64_t
decode_pointer(char **addr, uint64_t encoding)
{
    uint64_t result = 0;

    if (encoding == DW_EH_PE_omit)
        return 0;

    // The following are defined in the DWARF Exception Header Encodings
    // section 10.5.1. For some reason, GCC adds a 0x80 to the upper 4 bits
    // that are not documented in the LSB. Thefore, only 3 of the upper 4 bits
    // are actually used.

    switch (encoding & 0x70)
    {
        case DW_EH_PE_absptr:
            break;

        case DW_EH_PE_pcrel:
            result += (uint64_t) * addr;
            break;

        case DW_EH_PE_textrel:
            ABORT("DW_EH_PE_textrel pointer encodings not supported");
            break;

        case DW_EH_PE_datarel:
            ABORT("DW_EH_PE_datarel pointer encodings not supported");
            break;

        case DW_EH_PE_funcrel:
            ABORT("DW_EH_PE_funcrel pointer encodings not supported");
            break;

        case DW_EH_PE_aligned:
            ABORT("DW_EH_PE_aligned pointer encodings not supported");
            break;

        default:
            ABORT("unknown upper pointer encoding bits");
    }

    switch (encoding & 0x0F)
    {
        case DW_EH_PE_absptr:
            result += (uint64_t) * (void **)*addr;
            *addr += sizeof(void *);
            break;

        case DW_EH_PE_uleb128:
            result += (uint64_t)dwarf4::decode_uleb128(addr);
            break;

        case DW_EH_PE_udata2:
            result += (uint64_t) * (uint16_t *)*addr;
            *addr += sizeof(uint16_t);
            break;

        case DW_EH_PE_udata4:
            result += (uint64_t) * (uint32_t *)*addr;
            *addr += sizeof(uint32_t);
            break;

        case DW_EH_PE_udata8:
            result += (uint64_t) * (uint64_t *)*addr;
            *addr += sizeof(uint64_t);
            break;

        case DW_EH_PE_sleb128:
            result += (uint64_t)dwarf4::decode_sleb128(addr);
            break;

        case DW_EH_PE_sdata2:
            result += (uint64_t) * (int16_t *)*addr;
            *addr += sizeof(int16_t);
            break;

        case DW_EH_PE_sdata4:
            result += (uint64_t) * (int32_t *)*addr;
            *addr += sizeof(int32_t);
            break;

        case DW_EH_PE_sdata8:
            result += (uint64_t) * (int64_t *)*addr;
            *addr += sizeof(int64_t);
            break;

        default:
            ABORT("unknown lower pointer encoding bits");
    }

    return result;
}

// -----------------------------------------------------------------------------
// CIE / FDE Common
// -----------------------------------------------------------------------------

common_entry::common_entry() :
    m_is_cie(0),
    m_entry_start(0),
    m_entry_end(0),
    m_payload_start(0),
    m_payload_end(0),
    m_eh_frame{0, 0}
{
}

common_entry::common_entry(const eh_frame_t &eh_frame) :
    m_is_cie(0),
    m_entry_start(0),
    m_entry_end(0),
    m_payload_start(0),
    m_payload_end(0),
    m_eh_frame(eh_frame)
{
}

common_entry &common_entry::operator++()
{
    if (m_entry_start == 0)
        return *this;

    if (m_entry_end + 4 < (char *)m_eh_frame.addr + m_eh_frame.size)
        parse(m_entry_end);
    else
        parse(0);

    return *this;
}

void
common_entry::parse(char *addr)
{
    auto len = 0ULL;

    if ((m_entry_start = addr) == 0)
        goto failure;

    if (m_entry_start < m_eh_frame.addr)
        goto failure;

    if (((uint32_t *)(m_entry_start))[0] != 0xFFFFFFFF)
    {
        len = ((uint32_t *)(m_entry_start + 0))[0];
        m_payload_start = m_entry_start + 4;
    }
    else
    {
        len = ((uint64_t *)(m_entry_start + 4))[0];
        m_payload_start = m_entry_start + 12;
    }

    if (len == 0)
        goto failure;

    m_payload_end = m_payload_start + len;
    m_entry_end = m_payload_end;

    if (m_entry_end > (char *)m_eh_frame.addr + m_eh_frame.size)
        goto failure;

    m_is_cie = (((uint32_t *)(m_payload_start))[0] == 0);
    return;

failure:

    m_is_cie = false;
    m_entry_start = 0;
    m_entry_end = 0;
    m_payload_start = 0;
    m_payload_end = 0;
}

// -----------------------------------------------------------------------------
// Common Information Entry Record (CIE)
// -----------------------------------------------------------------------------

ci_entry::ci_entry() :
    m_augmentation_string(0),
    m_code_alignment(0),
    m_data_alignment(0),
    m_return_address_reg(0),
    m_pointer_encoding(0),
    m_lsda_encoding(0),
    m_personality_encoding(0),
    m_personality_function(0),
    m_initial_instructions(0)
{
}

ci_entry::ci_entry(const eh_frame_t &eh_frame) :
    common_entry(eh_frame),

    m_augmentation_string(0),
    m_code_alignment(0),
    m_data_alignment(0),
    m_return_address_reg(0),
    m_pointer_encoding(0),
    m_lsda_encoding(0),
    m_personality_encoding(0),
    m_personality_function(0),
    m_initial_instructions(0)
{
    parse((char *)eh_frame.addr);
}

ci_entry::ci_entry(const eh_frame_t &eh_frame, void *addr) :
    common_entry(eh_frame),

    m_augmentation_string(0),
    m_code_alignment(0),
    m_data_alignment(0),
    m_return_address_reg(0),
    m_pointer_encoding(0),
    m_lsda_encoding(0),
    m_personality_encoding(0),
    m_personality_function(0),
    m_initial_instructions(0)
{
    parse((char *)addr);
}

void
ci_entry::parse(char *addr)
{
    common_entry::parse(addr);

    if (*this == false)
        return;

    if (is_cie() == false)
        return;

    auto p = payload_start();

    p += sizeof(uint32_t);
    p += sizeof(uint8_t);

    m_augmentation_string = p;

    while (*p++ != 0);

    m_code_alignment = dwarf4::decode_uleb128(&p);
    m_data_alignment = dwarf4::decode_sleb128(&p);
    m_return_address_reg = dwarf4::decode_uleb128(&p);

    if (m_augmentation_string[0] == 'z')
    {
        auto len = dwarf4::decode_uleb128(&p);

        for (auto i = 1U; m_augmentation_string[i] != 0 && i <= len; i++)
        {
            switch (m_augmentation_string[i])
            {
                case 'L':
                    m_lsda_encoding = *(uint8_t *)p++;
                    break;

                case 'P':
                    m_personality_encoding = *(uint8_t *)p++;
                    m_personality_function =
                        decode_pointer(&p, m_personality_encoding);
                    break;

                case 'R':
                    m_pointer_encoding = *(uint8_t *)p++;
                    break;

                default:
                    ABORT("unknown augmentation string character");
            }
        }
    }

    m_initial_instructions = p;
}

// -----------------------------------------------------------------------------
// Frame Description Entry Record (FDE)
// -----------------------------------------------------------------------------

fd_entry::fd_entry() :
    m_pc_begin(0),
    m_pc_range(0),
    m_lsda(0),
    m_instructions(0)
{
}

fd_entry::fd_entry(const eh_frame_t &eh_frame) :
    common_entry(eh_frame),

    m_pc_begin(0),
    m_pc_range(0),
    m_lsda(0),
    m_instructions(0)
{
    parse((char *)eh_frame.addr);
}

fd_entry::fd_entry(const eh_frame_t &eh_frame, void *addr) :
    common_entry(eh_frame),

    m_pc_begin(0),
    m_pc_range(0),
    m_lsda(0),
    m_instructions(0)
{
    parse((char *)addr);
}

void
fd_entry::parse(char *addr)
{
    common_entry::parse(addr);

    if (*this == false)
        return;

    if (is_fde() == false)
        return;

    auto p = payload_start();
    auto p_cie = (char *)((uint64_t)p - * (uint32_t *)p);

    m_cie = ci_entry(eh_frame(), p_cie);
    p += sizeof(uint32_t);

    m_pc_begin = decode_pointer(&p, m_cie.pointer_encoding());
    m_pc_range = decode_pointer(&p, m_cie.pointer_encoding() & 0xF);

    if (m_cie.augmentation_string(0) == 'z')
    {
        auto len = dwarf4::decode_uleb128(&p);

        for (auto i = 1U; m_cie.augmentation_string(i) != 0 && i <= len; i++)
        {
            switch (m_cie.augmentation_string(i))
            {
                case 'L':
                    m_lsda = decode_pointer(&p, m_cie.lsda_encoding());
                    break;

                case 'P':
                    break;

                case 'R':
                    break;

                default:
                    ABORT("unknown augmentation string character");
            }
        }
    }

    m_instructions = p;
}

// -----------------------------------------------------------------------------
// Exception Handler Framework (eh_frame)
// -----------------------------------------------------------------------------

fd_entry
eh_frame::find_fde(register_state *state)
{
    auto eh_frame_list = get_eh_frame_list();

    for (auto m = 0U; m < MAX_NUM_MODULES; m++)
    {
        for (auto fde = fd_entry(eh_frame_list[m]); fde; ++fde)
        {
            if (fde.is_cie())
                continue;

            if (fde.is_in_range(state->get_ip()) == true)
            {
                if (g_phase == 1)
                {
                    log("\n");
                    debug("unwinder found rip: %p\n", (void *)state->get_ip());
                }

                return fde;
            }
        }
    }

    debug("ERROR: An exception was thrown, but the unwinder was unable to "
          "locate a stack frame for RIP = %p. Aborting!!!\n",
          (void *)state->get_ip());

    state->dump();

    return fd_entry();
}
