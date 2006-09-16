/*
 *  Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/**
 * @author Alexander V. Astapchuk
 * @version $Revision: $
 */
 
/**
 * @file
 * @brief Main decoding (disassembling) routines implementation.
 */

#include "dec_base.h"

bool DecoderBase::is_prefix(const unsigned char * bytes)
{
    unsigned char b0 = *bytes;
    unsigned char b1 = *(bytes+1);
    if (b0 == 0xF0) { // LOCK
        return true;
    }
    if (b0==0xF2 || b0==0xF3) { // REPNZ/REPZ prefixes
        if (b1 == 0x0F) {   // .... but may be a part of SIMD opcode
            return false;
        }
        return true;
    }
    if (b0 == 0x2E || b0 == 0x36 || b0==0x3E || b0==0x26 || b0==0x64 || b0==0x3E) {
        // branch hints, segment prefixes
        return true;
    }
    if (b0==0x66) { // operand-size prefix
        if (b1 == 0x0F) {   // .... but may be a part of SIMD opcode
            return false;
        }
        return true;
    }
    if (b0==0x67) { // address size prefix
        return true;
    }
    return false;
}


unsigned DecoderBase::decode(const void * addr, Inst * pinst)
{
    Inst tmp;
    
    const unsigned char * bytes = (unsigned char*)addr;
    // Check prefix first
    for (unsigned i=0; i<4; i++) {
        if (!is_prefix(bytes)) {
            break;
        }
        ++bytes;
    }

    if (is_prefix(bytes)) {
        // More than 4 prefixes together ?
        assert(false);
        return 0;
    }
    
    // Load up to 4 prefixes
    // for each Mnemonic
    //  for each opcodedesc
    //      if (raw_len == 0) memcmp(, raw_len)
    //  else check the mixed state which is one of the following:
    //      /digit /i /rw /rd /rb
    
    bool found = false;
    const unsigned char * saveBytes = bytes;
    for (unsigned mn=1; mn<Mnemonic_Count; mn++) {
        bytes = saveBytes;
        found=try_mn((Mnemonic)mn, &bytes, &tmp);
        if (found) {
            tmp.mn = (Mnemonic)mn;
            break;
        }
    }
    if (!found) {
        assert(false);
        return 0;
    }
    tmp.size = (unsigned)(bytes-(const unsigned char*)addr);
    if (pinst) {
        *pinst = tmp;
    }
    return tmp.size;
}

bool DecoderBase::decode_aux(const EncoderBase::OpcodeDesc& odesc, unsigned aux,
             const unsigned char ** pbuf, Inst * pinst) 
{
    OpcodeByteKind kind = (OpcodeByteKind)(aux & OpcodeByteKind_KindMask);
    unsigned byte = (aux & OpcodeByteKind_OpcodeMask);
    unsigned data_byte = **pbuf;
    switch (kind) {
    case OpcodeByteKind_SlashR:
        decodeModRM(odesc, pbuf, pinst);
        return true;
    case OpcodeByteKind_rb:
    case OpcodeByteKind_rw:
    case OpcodeByteKind_rd:
        {
            unsigned regid = data_byte - byte;
            if (regid>7) {
                return false;
            }
            ++*pbuf;
            return true;
        }
    case OpcodeByteKind_cb:
        pinst->offset = *(char*)*pbuf;
        *pbuf += 1;
        pinst->direct_addr = (void*)(pinst->offset + *pbuf);
        return true;
    case OpcodeByteKind_cw:
        assert(false); // not an error, but not expected in current env
        break;
    case OpcodeByteKind_cd:
        pinst->offset = *(int*)*pbuf;
        *pbuf += 4;
        pinst->direct_addr = (void*)(pinst->offset + *pbuf);
        return true;
    case OpcodeByteKind_SlashNum:
        {
        const ModRM& modrm = *(ModRM*)*pbuf;
        if (modrm.reg != byte) {
            return false;
        }
        decodeModRM(odesc, pbuf, pinst);
        }
        return true;
    case OpcodeByteKind_ib:
        *pbuf += 1;
        return true;
    case OpcodeByteKind_iw:
        *pbuf += 2;
        return true;
    case OpcodeByteKind_id:
        *pbuf += 4;
        return true;
    case OpcodeByteKind_plus_i:
        {
            unsigned regid = data_byte - byte;
            if (regid>7) {
                return false;
            }
            ++*pbuf;
            return true;
        }
    case OpcodeByteKind_ZeroOpcodeByte: // cant be here
        assert(false);
        break;
    default:
        // unknown kind ? how comes ?
        assert(false);
        break;
    }
    return false;
}

bool DecoderBase::try_mn(Mnemonic mn, const unsigned char ** pbuf, Inst * pinst) {
    const unsigned char * save_pbuf = *pbuf;
    EncoderBase::OpcodeDesc * opcodes = EncoderBase::opcodes[mn];
    for (unsigned i=0; !opcodes[i].last; i++) {
        const EncoderBase::OpcodeDesc& odesc = opcodes[i];
        *pbuf = save_pbuf;
        if (odesc.opcode_len != 0) {
            if (memcmp(*pbuf, odesc.opcode, odesc.opcode_len)) {
                continue;
            }
            *pbuf += odesc.opcode_len;
        }
        if (odesc.aux0 != 0) {
            
            if (!decode_aux(odesc, odesc.aux0, pbuf, pinst)) {
                continue;
            }
            if (odesc.aux1 != 0) {
                if (!decode_aux(odesc, odesc.aux1, pbuf, pinst)) {
                    continue;
                }
            }
            pinst->odesc = &opcodes[i];
            return true;
        }
        else {
            // Can't have empty opcode
            assert(odesc.opcode_len != 0);
            pinst->odesc = &opcodes[i];
            return true;
        }
    }
    return false;
}

bool DecoderBase::decodeModRM(const EncoderBase::OpcodeDesc& odesc,
                              const unsigned char ** pbuf, Inst * pinst)
{
    const ModRM& modrm = *(ModRM*)*pbuf;
    *pbuf += 1;
    if (modrm.mod == 3) {
        // we have only modrm. no sib, no disp.
        return true;
    }
    const SIB& sib = *(SIB*)*pbuf;
    // check whether we have a sib
    if (modrm.rm == 4) {
        // yes, we have SIB
        *pbuf += 1;
    }
    
    if (modrm.mod == 2) {
        // have disp32 
        *pbuf += 4;
    }
    else if (modrm.mod == 1) {
        // have disp8 
        *pbuf += 1;
    }
    else {
        assert(modrm.mod == 0);
        if (modrm.rm == 5) {
            // have disp32 w/o sib
            *pbuf += 4;
        }
        else if (modrm.rm == 4 && sib.base == 5) {
            // have to analyze sib, special case without EBP: have disp32+SI
            *pbuf += 4;
        }
    }
    return true;
}
