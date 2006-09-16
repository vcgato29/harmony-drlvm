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
 * @author Ivan Volosyuk
 */

#include <open/vm.h>
#include "gc_types.h"
#include "fast_list.h"
#include <assert.h>
#include <open/gc.h>
#include <open/types.h>

extern fast_list<Partial_Reveal_Object**,65536> slots;
typedef fast_list<Partial_Reveal_Object*,1024> reference_vector;
extern reference_vector finalizible_objects;
extern reference_vector soft_references;
extern reference_vector weak_references;
extern reference_vector phantom_references;
extern int soft_refs;
extern int weak_refs;
extern int phantom_refs;
extern int object_count;
typedef fast_list<unsigned char*,1024> pinned_areas_unsorted_t;
extern pinned_areas_unsorted_t pinned_areas_unsorted;

inline void add_reference_to_list(Partial_Reveal_Object* obj, 
        reference_vector& references)
{
    TRACE2("gc.debug", "0x" << obj << " referenced from weak reference <unknown>");
    references.push_back(obj);
}

inline void add_soft_reference(Partial_Reveal_Object *obj) {
    //TRACE2("gc.ref.alive", "add_soft_reference("<< obj << ")");
    add_reference_to_list(obj, soft_references);
}

inline void add_phantom_reference(Partial_Reveal_Object *obj) {
    //TRACE2("gc.ref.alive", "add_phantom_reference("<< obj << ")");
    add_reference_to_list(obj, phantom_references);
}

inline void add_weak_reference(Partial_Reveal_Object *obj) {
    //TRACE2("gc.ref.alive", "add_weak_reference("<< obj << ")");
    add_reference_to_list(obj, weak_references);
}

inline void assert_vt(Partial_Reveal_Object *obj) {
    assert(obj->vt() & ~(FORWARDING_BIT|RESCAN_BIT));
    assert(!(obj->vt() & (FORWARDING_BIT|RESCAN_BIT)));
}

void gc_copy_add_root_set_entry(Managed_Object_Handle *ref, Boolean is_pinned);
void gc_copy_add_root_set_entry_interior_pointer (void **slot, int offset, Boolean is_pinned);
void gc_copy_update_regions();

void gc_forced_add_root_set_entry(Managed_Object_Handle *ref, Boolean is_pinned);
void gc_forced_add_root_set_entry_interior_pointer (void **slot, int offset, Boolean is_pinned);

void gc_reset_interior_pointers();
void gc_process_interior_pointers();
void gc_slide_add_root_set_entry(Managed_Object_Handle *ref, Boolean is_pinned);
void gc_slide_add_root_set_entry_interior_pointer (void **slot, int offset, Boolean is_pinned);
void gc_slide_move_all();
void gc_slide_process_special_references(reference_vector& array);
void gc_slide_postprocess_special_references(reference_vector& array);

void transition_copy_to_sliding_compaction(fast_list<Partial_Reveal_Object**,65536>& slots);
void gc_slide_process_transitional_slots(fast_list<Partial_Reveal_Object**,65536>& slots);
void gc_slide_process_transitional_slots(Partial_Reveal_Object **refs, int pos, int length);

void gc_cache_add_root_set_entry(Managed_Object_Handle *ref, Boolean is_pinned);
void gc_cache_add_root_set_entry_interior_pointer (void **slot, int offset, Boolean is_pinned);
void gc_cache_retrieve_root_set();
void gc_cache_emit_root_set();

void gc_forced_mt_mark_scan();