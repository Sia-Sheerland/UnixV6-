#include "PageRefCount.h"

unsigned char PageRefCount::s_counts[MAX_USER_PAGES];

void PageRefCount::Inc(unsigned long physAddr)
{
    int i = Idx(physAddr);
    if (i >= 0 && s_counts[i] < 255)
        s_counts[i]++;
}

void PageRefCount::Dec(unsigned long physAddr)
{
    int i = Idx(physAddr);
    if (i >= 0 && s_counts[i] > 0)
        s_counts[i]--;
}

unsigned char PageRefCount::Get(unsigned long physAddr)
{
    int i = Idx(physAddr);
    if (i >= 0) return s_counts[i];
    return 0;
}

void PageRefCount::Set(unsigned long physAddr, unsigned char val)
{
    int i = Idx(physAddr);
    if (i >= 0) s_counts[i] = val;
}
