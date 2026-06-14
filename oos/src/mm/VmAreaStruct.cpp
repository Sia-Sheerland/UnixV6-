#include "VmAreaStruct.h"

/* 静态 VmAreaStruct 池 */
static VmAreaStruct g_VmaPool[VMA_POOL_SIZE];
static bool         g_VmaUsed[VMA_POOL_SIZE];

VmAreaStruct* VmaAlloc()
{
    for (int i = 0; i < VMA_POOL_SIZE; i++) {
        if (!g_VmaUsed[i]) {
            g_VmaUsed[i] = true;
            VmAreaStruct* v = &g_VmaPool[i];
            v->vm_start  = 0;
            v->vm_end    = 0;
            v->vm_prot   = 0;
            v->f_start   = 0;
            v->f_len     = 0;
            v->vm_flags  = 0;
            v->vm_inode  = 0;
            v->next      = 0;
            return v;
        }
    }
    return 0; /* 池已满 */
}

void VmaFree(VmAreaStruct* vma)
{
    if (!vma) return;
    for (int i = 0; i < VMA_POOL_SIZE; i++) {
        if (&g_VmaPool[i] == vma) {
            g_VmaUsed[i] = false;
            return;
        }
    }
}

VmAreaStruct* VmaCloneList(VmAreaStruct* head)
{
    VmAreaStruct* newHead = 0;
    VmAreaStruct* newTail = 0;

    for (VmAreaStruct* src = head; src; src = src->next) {
        VmAreaStruct* dst = VmaAlloc();
        if (!dst) break; /* 池满，截断 */

        dst->vm_start = src->vm_start;
        dst->vm_end   = src->vm_end;
        dst->vm_prot  = src->vm_prot;
        dst->f_start  = src->f_start;
        dst->f_len    = src->f_len;
        dst->vm_flags = src->vm_flags;
        dst->vm_inode = src->vm_inode;   /* 同一 inode（由调用者递增引用计数） */
        dst->next     = 0;

        if (!newHead) {
            newHead = newTail = dst;
        } else {
            newTail->next = dst;
            newTail = dst;
        }
    }
    return newHead;
}
