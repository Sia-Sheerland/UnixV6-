#ifndef VM_AREA_STRUCT_H
#define VM_AREA_STRUCT_H

/*
 * VmAreaStruct — 虚拟内存区域描述符
 * 对应 Linux 的 vm_area_struct，描述进程地址空间中的一个连续区域。
 */

/* vm_prot 保护位 */
#define VM_PROT_READ   0x1
#define VM_PROT_WRITE  0x2
#define VM_PROT_EXEC   0x4

/* vm_flags 区域类型 */
#define VM_TEXT    0x01   /* 代码段，只读，文件映射 */
#define VM_DATA    0x02   /* 数据段，读写，文件映射 */
#define VM_BSS     0x04   /* BSS 段，读写，匿名（清零） */
#define VM_HEAP    0x08   /* 堆，读写，匿名（惰性清零） */
#define VM_STACK   0x10   /* 用户栈，读写，匿名（惰性清零） */
#define VM_ANON    0x20   /* 匿名映射（无文件支持） */

/* 静态 VmAreaStruct 池大小 */
#define VMA_POOL_SIZE 256

/* 前置声明（避免循环依赖） */
class Inode;

struct VmAreaStruct
{
    unsigned long  vm_start;  /* 区域起始虚拟地址（含） */
    unsigned long  vm_end;    /* 区域结束虚拟地址（不含） */
    unsigned char  vm_prot;   /* 保护位：VM_PROT_READ / WRITE / EXEC */
    unsigned long  f_start;   /* 对应文件中的起始偏移（字节） */
    unsigned long  f_len;     /* 需从文件读入的字节数（余下清零） */
    unsigned int   vm_flags;  /* 区域类型：VM_TEXT / DATA / BSS / HEAP / STACK */
    Inode*         vm_inode;  /* 支撑文件的 inode，匿名映射为 NULL */
    VmAreaStruct*  next;      /* 进程 VMA 链表的下一个节点 */
};

/* 从静态池分配/释放 VmAreaStruct */
VmAreaStruct* VmaAlloc();
void VmaFree(VmAreaStruct* vma);

/* 深拷贝一条 VMA 链表（不递增 inode 引用计数，由调用者管理） */
VmAreaStruct* VmaCloneList(VmAreaStruct* head);

#endif
