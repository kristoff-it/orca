/*************************************************************************
*
*  Orca
*  Copyright 2023 Martin Fouilleul and the Orca project contributors
*  See LICENSE.txt for licensing information
*
**************************************************************************/
#include <limits.h>
#include "runtime.h"
#include "runtime_memory.h"

void* oc_wasm_memory_resize_callback(void* p, unsigned long newSize, void* userData)
{
    //NOTE: this is called by wasm3. The size passed includes wasm3 memory header.
    //      We first align it on 4K page size

    newSize = oc_align_up_pow2(newSize, 4 << 10);

    oc_wasm_memory* memory = (oc_wasm_memory*)userData;

    if(memory->committed >= newSize)
    {
        return (memory->ptr);
    }
    else if(newSize <= memory->reserved)
    {
        u32 commitSize = newSize - memory->committed;

        oc_base_allocator* allocator = oc_base_allocator_default();
        oc_base_commit(allocator, memory->ptr + memory->committed, commitSize);
        memory->committed += commitSize;

        OC_DEBUG_ASSERT((memory->committed & 0xfff) == 0, "Committed pointer is not aligned on page size");

        return (memory->ptr);
    }
    else
    {
        OC_ABORT("Out of memory");
        return (0);
    }
}

void oc_wasm_memory_free_callback(void* p, void* userData)
{
    oc_wasm_memory* memory = (oc_wasm_memory*)userData;

    oc_base_allocator* allocator = oc_base_allocator_default();
    oc_base_release(allocator, memory->ptr, memory->reserved);
    memset(memory, 0, sizeof(oc_wasm_memory));
}

extern u32 oc_mem_grow(u64 size)
{
    oc_wasm_env* env = oc_runtime_get_env();
    oc_wasm_memory* memory = &env->wasmMemory;

    u32 oldMemSize = m3_GetMemorySize(env->m3Runtime);

    //NOTE: compute total size and align on wasm memory page size
    OC_ASSERT(oldMemSize <= UINT_MAX - size, "Memory size overflow");
    u64 newMemSize = size + oldMemSize;

    newMemSize = oc_align_up_pow2(newMemSize, d_m3MemPageSize);

    //NOTE: call resize memory, which will call our custom resize callback... this is a bit involved because
    //      wasm3 doesn't allow resizing the memory directly
    M3Result res = ResizeMemory(env->m3Runtime, newMemSize / d_m3MemPageSize);

    OC_DEBUG_ASSERT(oldMemSize + size <= m3_GetMemorySize(env->m3Runtime), "Memory returned by oc_mem_grow overflows wasm memory");

    return (oldMemSize);
}

void* oc_wasm_address_to_ptr(oc_wasm_addr addr, oc_wasm_size size)
{
    oc_str8 mem = oc_runtime_get_wasm_memory();
    OC_ASSERT(addr + size < mem.len, "Object overflows wasm memory");

    void* ptr = (addr == 0) ? 0 : mem.ptr + addr;
    return (ptr);
}

oc_wasm_addr oc_wasm_address_from_ptr(void* ptr, oc_wasm_size size)
{
    oc_wasm_addr addr = 0;
    if(ptr != 0)
    {
        oc_str8 mem = oc_runtime_get_wasm_memory();
        OC_ASSERT((char*)ptr > mem.ptr && (((char*)ptr - mem.ptr) + size < mem.len), "Object overflows wasm memory");

        addr = (char*)ptr - mem.ptr;
    }
    return (addr);
}

//------------------------------------------------------------------------------------
// oc_wasm_list helpers
//------------------------------------------------------------------------------------

void oc_wasm_list_push(oc_wasm_list* list, oc_wasm_list_elt* elt)
{
    elt->next = list->first;
    elt->prev = 0;

    oc_wasm_addr eltAddr = oc_wasm_address_from_ptr(elt, sizeof(oc_wasm_list_elt));

    if(list->first)
    {
        oc_wasm_list_elt* first = oc_wasm_address_to_ptr(list->first, sizeof(oc_wasm_list_elt));
        first->prev = eltAddr;
    }
    else
    {
        list->last = eltAddr;
    }
    list->first = eltAddr;
}

void oc_wasm_list_push_back(oc_wasm_list* list, oc_wasm_list_elt* elt)
{
    elt->prev = list->last;
    elt->next = 0;

    oc_wasm_addr eltAddr = oc_wasm_address_from_ptr(elt, sizeof(oc_wasm_list_elt));

    if(list->last)
    {
        oc_wasm_list_elt* last = oc_wasm_address_to_ptr(list->last, sizeof(oc_wasm_list_elt));
        last->next = eltAddr;
    }
    else
    {
        list->first = eltAddr;
    }
    list->last = eltAddr;
}

//------------------------------------------------------------------------------------
// Wasm arenas helpers
//------------------------------------------------------------------------------------

oc_wasm_addr oc_wasm_arena_push(oc_wasm_addr arena, u64 size)
{
    oc_wasm_env* env = oc_runtime_get_env();

    oc_wasm_addr retValues[1] = { 0 };
    const void* retPointers[1] = { (void*)&retValues[0] };
    const void* args[2] = { &arena, &size };

    M3Result res = m3_Call(env->exports[OC_EXPORT_ARENA_PUSH], 2, args);
    if(res)
    {
        ORCA_WASM3_ABORT(env->m3Runtime, res, "Runtime error");
    }

    res = m3_GetResults(env->exports[OC_EXPORT_ARENA_PUSH], 1, retPointers);
    if(res)
    {
        ORCA_WASM3_ABORT(env->m3Runtime, res, "Runtime error");
    }

    return (retValues[0]);
}
