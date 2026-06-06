#include "MemoryDescriptor.h"
#include "Kernel.h"
#include "PageManager.h"
#include "Machine.h"
#include "PageDirectory.h"
#include "Video.h"
#include "OpenFileManager.h"

/* ================================================================
 * 生命周期
 * ================================================================ */

MemoryDescriptor::MemoryDescriptor()
    : m_UserPageTableArray(0),
      m_TextStartAddress(0), m_TextSize(0),
      m_DataStartAddress(0), m_DataSize(0),
      m_StackSize(0),
      m_HeapStart(0), m_HeapEnd(0),
      m_VmaList(0)
{
}

MemoryDescriptor::~MemoryDescriptor()
{
}

void MemoryDescriptor::Initialize()
{
    KernelPageManager& kpm = Kernel::Instance().GetKernelPageManager();
    this->m_UserPageTableArray = (PageTable*)(
        kpm.AllocMemory(sizeof(PageTable) * USER_SPACE_PAGE_TABLE_CNT)
        + Machine::KERNEL_SPACE_START_ADDRESS);
}

void MemoryDescriptor::Release()
{
    KernelPageManager& kpm = Kernel::Instance().GetKernelPageManager();
    if (this->m_UserPageTableArray) {
        kpm.FreeMemory(sizeof(PageTable) * USER_SPACE_PAGE_TABLE_CNT,
                       (unsigned long)this->m_UserPageTableArray
                       - Machine::KERNEL_SPACE_START_ADDRESS);
        this->m_UserPageTableArray = 0;
    }
}

/* ================================================================
 * VMA 管理
 * ================================================================ */

VmAreaStruct* MemoryDescriptor::AddVma(unsigned long start, unsigned long end,
                                       unsigned char prot, unsigned int flags,
                                       Inode* inode,
                                       unsigned long f_start, unsigned long f_len)
{
    VmAreaStruct* vma = VmaAlloc();
    if (!vma) return 0;

    vma->vm_start = start;
    vma->vm_end   = end;
    vma->vm_prot  = prot;
    vma->vm_flags = flags;
    vma->vm_inode = inode;
    vma->f_start  = f_start;
    vma->f_len    = f_len;

    /* 插入链表头部 */
    vma->next    = this->m_VmaList;
    this->m_VmaList = vma;
    return vma;
}

VmAreaStruct* MemoryDescriptor::FindVma(unsigned long va)
{
    for (VmAreaStruct* v = this->m_VmaList; v; v = v->next) {
        if (va >= v->vm_start && va < v->vm_end)
            return v;
    }
    return 0;
}

void MemoryDescriptor::FreeAllVmas()
{
    InodeTable* inodeTbl = Kernel::Instance().GetFileManager().m_InodeTable;
    VmAreaStruct* v = this->m_VmaList;
    while (v) {
        VmAreaStruct* next = v->next;
        /* 若 VMA 持有 inode 引用，释放之 */
        if (v->vm_inode) {
            inodeTbl->IPut(v->vm_inode);
            v->vm_inode = 0;
        }
        VmaFree(v);
        v = next;
    }
    this->m_VmaList = 0;
}

VmAreaStruct* MemoryDescriptor::CloneVmaList()
{
    VmAreaStruct* newHead = 0;
    VmAreaStruct* newTail = 0;

    for (VmAreaStruct* src = this->m_VmaList; src; src = src->next) {
        VmAreaStruct* dst = VmaAlloc();
        if (!dst) break;

        dst->vm_start = src->vm_start;
        dst->vm_end   = src->vm_end;
        dst->vm_prot  = src->vm_prot;
        dst->f_start  = src->f_start;
        dst->f_len    = src->f_len;
        dst->vm_flags = src->vm_flags;
        dst->vm_inode = src->vm_inode;
        dst->next     = 0;

        /* 若持有 inode，增加引用计数 */
        if (dst->vm_inode)
            dst->vm_inode->i_count++;

        if (!newHead) {
            newHead = newTail = dst;
        } else {
            newTail->next = dst;
            newTail = dst;
        }
    }
    return newHead;
}

/* ================================================================
 * 单页操作
 * ================================================================ */

PageTableEntry& MemoryDescriptor::GetPTE(unsigned long va)
{
    unsigned int pageNum  = va >> 12;
    unsigned int tableIdx = pageNum >> 10;   /* 0 或 1 */
    unsigned int entryIdx = pageNum & 0x3FF; /* 0 ~ 1023 */
    return this->m_UserPageTableArray[tableIdx].m_Entrys[entryIdx];
}

void MemoryDescriptor::MapPageDirect(unsigned long va, unsigned long physFrame, bool writable)
{
    /* 更新 shadow 页表 */
    PageTableEntry& spte = GetPTE(va);
    spte.m_Present            = 1;
    spte.m_ReadWriter         = writable ? 1 : 0;
    spte.m_UserSupervisor     = 1;
    spte.m_WriteThrough       = 0;
    spte.m_CacheDisabled      = 0;
    spte.m_Accessed           = 0;
    spte.m_Dirty              = 0;
    spte.m_GlobalPage         = 0;
    spte.m_PageBaseAddress    = physFrame;

    /* 同步更新硬件页表 */
    PageTable* hwPT = Machine::Instance().GetUserPageTableArray();
    unsigned int pageNum  = va >> 12;
    unsigned int tableIdx = pageNum >> 10;
    unsigned int entryIdx = pageNum & 0x3FF;
    hwPT[tableIdx].m_Entrys[entryIdx] = spte;
}

void MemoryDescriptor::UnmapPage(unsigned long va)
{
    /* shadow */
    PageTableEntry& spte = GetPTE(va);
    spte.m_Present        = 0;
    spte.m_ReadWriter     = 0;
    spte.m_PageBaseAddress= 0;

    /* 硬件 */
    PageTable* hwPT = Machine::Instance().GetUserPageTableArray();
    unsigned int pageNum  = va >> 12;
    unsigned int tableIdx = pageNum >> 10;
    unsigned int entryIdx = pageNum & 0x3FF;
    hwPT[tableIdx].m_Entrys[entryIdx] = spte;
    FlushPageDirectory();
}

/* ================================================================
 * MapToPageTable — 将 shadow 页表直接复制到硬件页表
 * （取代原先使用相对偏移的版本）
 * ================================================================ */
void MemoryDescriptor::MapToPageTable()
{
    User& u = Kernel::Instance().GetUser();
    if (!u.u_MemoryDescriptor.m_UserPageTableArray) return;

    PageTable* hwPT = Machine::Instance().GetUserPageTableArray();
    for (unsigned int i = 0; i < Machine::USER_PAGE_TABLE_CNT; i++) {
        for (unsigned int j = 0; j < PageTable::ENTRY_CNT_PER_PAGETABLE; j++) {
            hwPT[i].m_Entrys[j] = u.u_MemoryDescriptor.m_UserPageTableArray[i].m_Entrys[j];
        }
    }
    FlushPageDirectory();
}

void MemoryDescriptor::DisplayPageTable()
{
    unsigned int i, j;
    Diagnose::Write("Process PT:");
    for (i = 0; i < Machine::USER_PAGE_TABLE_CNT; i++)
        for (j = 0; j < PageTable::ENTRY_CNT_PER_PAGETABLE; j++)
            if (this->m_UserPageTableArray[i].m_Entrys[j].m_Present)
                Diagnose::Write("<%d,%x>  ", i * 1024 + j,
                    this->m_UserPageTableArray[i].m_Entrys[j].m_PageBaseAddress);
    Diagnose::Write("\n");
}

/* ================================================================
 * ClearUserPageTable
 * ================================================================ */
void MemoryDescriptor::ClearUserPageTable()
{
    User& u = Kernel::Instance().GetUser();
    PageTable* pt = u.u_MemoryDescriptor.m_UserPageTableArray;
    if (!pt) return;

    for (unsigned int i = 0; i < Machine::USER_PAGE_TABLE_CNT; i++) {
        for (unsigned int j = 0; j < PageTable::ENTRY_CNT_PER_PAGETABLE; j++) {
            pt[i].m_Entrys[j].m_Present          = 0;
            pt[i].m_Entrys[j].m_ReadWriter        = 0;
            pt[i].m_Entrys[j].m_UserSupervisor    = 1;
            pt[i].m_Entrys[j].m_PageBaseAddress   = 0;
        }
    }
}

/* ================================================================
 * EstablishUserPageTable
 *
 * 新语义：
 *   - 清空 shadow 页表（所有 P=0）
 *   - 更新段信息字段
 *   - 将代码段页映射到共享 x_caddr（只读）
 *   - 数据/BSS/栈/堆留作按需调页，VMA 由 exec/SStack/SBreak 负责添加
 *   - 调用 MapToPageTable() 同步到硬件
 * ================================================================ */
bool MemoryDescriptor::EstablishUserPageTable(unsigned long textVA,  unsigned long textSize,
                                               unsigned long dataVA,  unsigned long dataSize,
                                               unsigned long stackSize)
{
    User& u = Kernel::Instance().GetUser();

    if (textSize + dataSize + stackSize + PageManager::PAGE_SIZE
        > USER_SPACE_SIZE - textVA) {
        u.u_error = User::ENOMEM;
        return false;
    }

    /* 更新段描述信息 */
    this->m_TextStartAddress = textVA;
    this->m_TextSize         = textSize;
    this->m_DataStartAddress = dataVA;
    this->m_DataSize         = dataSize;
    this->m_StackSize        = stackSize;

    /* 清空所有页表项 */
    this->ClearUserPageTable();

    /* 映射代码段（只读，绝对帧号 = x_caddr 中的帧） */
    if (u.u_procp->p_textp && textSize > 0) {
        unsigned long textPhysBase = u.u_procp->p_textp->x_caddr;
        unsigned long pages = (textSize + PageManager::PAGE_SIZE - 1)
                              / PageManager::PAGE_SIZE;
        for (unsigned long p = 0; p < pages; p++) {
            unsigned long va  = textVA + p * PageManager::PAGE_SIZE;
            unsigned long frm = (textPhysBase + p * PageManager::PAGE_SIZE)
                                >> 12;
            MapPageDirect(va, frm, false /* R/O */);
        }
    }

    /* 数据/栈/堆页：全部 P=0，等待缺页时按需分配 */

    /* 同步到硬件 */
    MapToPageTable();
    return true;
}

/* ================================================================
 * 旧接口兼容实现（MapTextEntrys / MapDataEntrys / MapStackEntrys）
 * 现在直接操作 shadow 页表的绝对帧号，不再用相对偏移。
 * ================================================================ */

unsigned int MemoryDescriptor::MapEntry(unsigned long va, unsigned int size,
                                        unsigned long phyPageIdx, bool isReadWrite)
{
    unsigned long startIdx = va >> 12;
    unsigned long cnt = (size + PageManager::PAGE_SIZE - 1) / PageManager::PAGE_SIZE;
    PageTableEntry* entrys = (PageTableEntry*)this->m_UserPageTableArray;
    for (unsigned long i = startIdx; i < startIdx + cnt; i++, phyPageIdx++) {
        entrys[i].m_Present          = 1;
        entrys[i].m_ReadWriter       = isReadWrite ? 1 : 0;
        entrys[i].m_UserSupervisor   = 1;
        entrys[i].m_PageBaseAddress  = phyPageIdx;
    }
    return phyPageIdx;
}

void MemoryDescriptor::MapTextEntrys(unsigned long addr, unsigned long size, unsigned long idx)
{
    MapEntry(addr, size, idx, false);
}
void MemoryDescriptor::MapDataEntrys(unsigned long addr, unsigned long size, unsigned long idx)
{
    MapEntry(addr, size, idx, true);
}
void MemoryDescriptor::MapStackEntrys(unsigned long stackSize, unsigned long idx)
{
    unsigned long stackStart = (USER_SPACE_START_ADDRESS + USER_SPACE_SIZE - stackSize)
                               & 0xFFFFF000;
    MapEntry(stackStart, stackSize, idx, true);
}

/* ================================================================
 * Getters
 * ================================================================ */
PageTable*    MemoryDescriptor::GetUserPageTableArray()  { return m_UserPageTableArray; }
unsigned long MemoryDescriptor::GetTextStartAddress()    { return m_TextStartAddress; }
unsigned long MemoryDescriptor::GetTextSize()            { return m_TextSize; }
unsigned long MemoryDescriptor::GetDataStartAddress()    { return m_DataStartAddress; }
unsigned long MemoryDescriptor::GetDataSize()            { return m_DataSize; }
unsigned long MemoryDescriptor::GetStackSize()           { return m_StackSize; }
