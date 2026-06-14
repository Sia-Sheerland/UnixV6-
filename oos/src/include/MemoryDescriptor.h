#ifndef MEMORY_DESCRIPTOR_H
#define MEMORY_DESCRIPTOR_H

#include "PageTable.h"
#include "VmAreaStruct.h"

/*
 * MemoryDescriptor — 进程虚拟地址空间描述符
 *
 * 在原 Unix V6++ 基础上扩展：
 *   1. 增加 vm_area_struct 链表，描述各虚拟内存区域；
 *   2. 增加堆的起止地址（支持 brk/sbrk）；
 *   3. 页表项改为存储绝对物理帧号，MapToPageTable() 直接复制；
 *   4. 新增 per-page 操作辅助函数，供缺页处理程序使用。
 */
class MemoryDescriptor
{
public:
    /* 用户空间大小 8M，使用 2 个页表 */
    static const unsigned int USER_SPACE_SIZE              = 0x800000;
    static const unsigned int USER_SPACE_PAGE_TABLE_CNT   = 0x2;
    static const unsigned long USER_SPACE_START_ADDRESS   = 0x0;

public:
    MemoryDescriptor();
    ~MemoryDescriptor();

public:
    /* -------- 生命周期 -------- */

    /* 分配并初始化 m_UserPageTableArray，挂入页目录前调用 */
    void Initialize();

    /* 释放进程页表所占内核内存（Exit 时调用） */
    void Release();

    /* -------- VMA 管理 -------- */

    /**
     * 向链表头部插入一个新 VMA。
     * @param start    虚拟起始地址
     * @param end      虚拟结束地址（不含）
     * @param prot     保护位（VM_PROT_*）
     * @param flags    区域类型（VM_TEXT / DATA / BSS / HEAP / STACK）
     * @param inode    文件支撑 inode（匿名为 NULL）
     * @param f_start  文件内起始偏移（字节）
     * @param f_len    需从文件读取的字节数
     * @return 新分配的 VmAreaStruct 指针，失败返回 NULL
     */
    VmAreaStruct* AddVma(unsigned long start, unsigned long end,
                         unsigned char prot, unsigned int flags,
                         Inode* inode, unsigned long f_start, unsigned long f_len);

    /**
     * 在 VMA 链表中查找包含虚拟地址 va 的区域。
     * @return 找到的 VmAreaStruct，找不到返回 NULL
     */
    VmAreaStruct* FindVma(unsigned long va);

    /**
     * 释放 VMA 链表中的所有节点；若节点持有 inode 引用则调用 IPut。
     */
    void FreeAllVmas();

    /**
     * 深拷贝 VMA 链表（用于 fork）；同时为每个持有 inode 的 VMA 增加引用计数。
     */
    VmAreaStruct* CloneVmaList();

    /* -------- 单页操作 -------- */

    /**
     * 获取虚拟地址 va 对应的页表项引用（shadow 页表）。
     */
    PageTableEntry& GetPTE(unsigned long va);

    /**
     * 在 shadow 页表和硬件页表中同时映射一个页。
     * @param va        虚拟地址（按页对齐）
     * @param physFrame 绝对物理帧号（physAddr >> 12）
     * @param writable  true = R/W，false = R/O
     */
    void MapPageDirect(unsigned long va, unsigned long physFrame, bool writable);

    /**
     * 取消 va 对应的映射（shadow + 硬件）。
     */
    void UnmapPage(unsigned long va);

    /**
     * 将 shadow 页表直接复制到硬件页表并刷新 TLB。
     * （在上下文切换时调用，取代原先带偏移的 MapToPageTable）
     */
    void MapToPageTable();

    void DisplayPageTable();

    /* -------- 整体布局建立（exec 调用） -------- */

    /**
     * 清空 shadow 页表中的所有项（P=0）。
     */
    void ClearUserPageTable();

    /**
     * 建立用户地址空间布局：
     *   - 根据 text/data/stack 参数创建 VMA；
     *   - 将代码段页映射到共享 x_caddr（已由 exec 加载完毕）；
     *   - 数据/BSS/堆/栈页均标记为未映射（P=0），等待缺页调入。
     *
     * 注意：调用者须在调用前已调用 FreeAllVmas() 清理旧映射。
     */
    bool EstablishUserPageTable(unsigned long textVirtualAddress,
                                unsigned long textSize,
                                unsigned long dataVirtualAddress,
                                unsigned long dataSize,
                                unsigned long stackSize);

    /* -------- 旧接口（保留以兼容 MapToPageTable 相关调用路径） -------- */
    void MapTextEntrys(unsigned long textStartAddress, unsigned long textSize,
                       unsigned long textPageIdxInPhyMemory);
    void MapDataEntrys(unsigned long dataStartAddress, unsigned long dataSize,
                       unsigned long dataPageIdxInPhyMemory);
    void MapStackEntrys(unsigned long stackSize, unsigned long stackPageIdxInPhyMemory);

    /* -------- Getters -------- */
    PageTable*    GetUserPageTableArray();
    unsigned long GetTextStartAddress();
    unsigned long GetTextSize();
    unsigned long GetDataStartAddress();
    unsigned long GetDataSize();
    unsigned long GetStackSize();

private:
    /* 旧的 per-page 映射辅助（保留，但 EstablishUserPageTable 已不再依赖） */
    unsigned int MapEntry(unsigned long virtualAddress, unsigned int size,
                          unsigned long phyPageIdx, bool isReadWrite);

public:
    /* -------- 页表 -------- */
    PageTable*     m_UserPageTableArray;   /* shadow 页表（内核内存） */

    /* -------- 段信息（兼容旧代码） -------- */
    unsigned long  m_TextStartAddress;
    unsigned long  m_TextSize;
    unsigned long  m_DataStartAddress;
    unsigned long  m_DataSize;
    unsigned long  m_StackSize;

    /* -------- 堆 -------- */
    unsigned long  m_HeapStart;   /* 堆的起始虚拟地址（由 exec 设置） */
    unsigned long  m_HeapEnd;     /* 当前 brk 指针（堆尾，随 sbrk/brk 变化） */

    /* -------- VMA 链表 -------- */
    VmAreaStruct*  m_VmaList;     /* 进程 VMA 链表头 */
};

#endif
