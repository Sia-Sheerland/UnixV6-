#ifndef PAGE_REF_COUNT_H
#define PAGE_REF_COUNT_H

/*
 * PageRefCount — 用户物理页框引用计数
 * 用于写时复制（COW）：当引用计数 > 1 时写操作触发页框复制。
 *
 * 用户物理页池从 0x400000 开始，最多跟踪 MAX_USER_PAGES 个页框。
 */

#define USER_PHY_PAGE_START  0x400000u
#define PAGE_SIZE_4K         0x1000u
#define MAX_USER_PAGES       8192

class PageRefCount
{
public:
    /* 增加物理地址 physAddr 所在页框的引用计数 */
    static void   Inc(unsigned long physAddr);
    /* 减少引用计数（不低于 0） */
    static void   Dec(unsigned long physAddr);
    /* 读取引用计数 */
    static unsigned char Get(unsigned long physAddr);
    /* 直接设置引用计数 */
    static void   Set(unsigned long physAddr, unsigned char val);

private:
    static unsigned char s_counts[MAX_USER_PAGES];

    /* 物理地址 → 数组下标 */
    static int Idx(unsigned long physAddr)
    {
        int i = (int)((physAddr - USER_PHY_PAGE_START) >> 12);
        if (i < 0 || i >= MAX_USER_PAGES) return -1;
        return i;
    }
};

#endif
