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
 * @author Intel, Konstantin M. Anisimov, Igor V. Chebykin
 * @version $Revision$
 *
 */

#ifndef IPFREGISTERALLOCATOR_H_
#define IPFREGISTERALLOCATOR_H_

#include "IpfCfg.h"

namespace Jitrino {
namespace IPF {

//========================================================================================//
// RegisterAllocator
//========================================================================================//

class RegisterAllocator {
public:
                  RegisterAllocator(Cfg&);
    void          allocate();

protected:
    void          buildInterferenceMatrix();
    void          makeInterferenceMatrixSymmetric();
    void          removePreassignedOpnds();
    void          assignLocations();
    
    void          updateAllocSet(Opnd*, uint32);
    void          checkCallSite(Inst*);

    MemoryManager &mm;
    Cfg           &cfg;
    OpndManager   *opndManager;
    RegOpndSet    allocSet;       // set of all opnds that need allocation
    RegOpndSet    liveSet;        // set of opnds alive in current node (buildInterferenceMatrix)
};

} // IPF
} // Jitrino

#endif /*IPFREGISTERALLOCATOR_H_*/