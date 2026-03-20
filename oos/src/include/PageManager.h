#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include "MapNode.h"
#include "Allocator.h"
#include "MemoryDescriptor.h"

class PageManager
{
public:
	/* static member */
	static unsigned int PHY_MEM_SIZE; /* �����ڴ��С��ϵͳ����ʱ���������ڴ��С���� */

	/* static const member */
	static const unsigned int PAGE_SIZE = 0x1000;				/* �����ڴ�ҳ��С */
	static const unsigned int MEMORY_MAP_ARRAY_SIZE = 0x200;	/* ���ɷ���512������ */
	static const unsigned int KERNEL_MEM_START_ADDR = 0x100000; /* �ں�ӳ���1M�����ڴ濪ʼ */
	static const unsigned int KERNEL_SIZE = 0x80000;			/* �ں�ӳ���С����(һ�������ӳ��Զ���ᵽ512K��С) */

	/* Functions */
public:
	PageManager(BitMapAllocator *allocator);
	virtual ~PageManager();

	/* ��ɶ�MapNode map[]����ĳ�ʼ������ */
	int Initialize();
	/*
	 * �����ڴ����
	 *
	 * size: ������ڴ��С(��λ: byte)��ʵ�ʷ��������ڴ��С��ҳ
	 * Ϊ��λ�������size��С��4KΪ�߽磬����ȡ����4K�ֽ���������
	 *
	 * ����ֵ: �ɹ�����������ڴ�����ʼ��ַ������0��ʾ����ʧ�ܡ�
	 */
	unsigned long AllocMemory(unsigned long size);
	/*
	 * �����ڴ��ͷ�
	 *
	 * size: ���ͷ��ڴ��С(��λ: byte)��ʵ���ͷ������ڴ��С��ҳ
	 * Ϊ��λ�������size��С��4KΪ�߽磬����ȡ����4K�ֽ���������
	 *
	 * ����ֵ: �ͷ������ڴ�������ܳɹ�����ͨ��������䷵��ֵ��
	 */
	unsigned long FreeMemory(unsigned long size, unsigned long memoryStartAddress);

private:
	PageManager();

	/* Members */
public:
	MapNode map[PageManager::MEMORY_MAP_ARRAY_SIZE];

	// NOTE:1
	BitMap bitmap;

	// NOTE:COW - reference counting array for COW
	int Page[MemoryDescriptor::USER_SPACE_SIZE / PAGE_SIZE];

private:
	BitMapAllocator *m_pAllocator;
};

class KernelPageManager : public PageManager
{
public:
	/*
	 * ������ַ 0x200000 ������PageDirectory,
	 * ������ַ 0x201000 �������ں�ҳ��,
	 * ������ַ 0x202000 �� 0x203000 �����û�����ҳ��.
	 */
	static const unsigned int KERNEL_PAGE_POOL_START_ADDR = 0x200000 + 0x2000 + 0x2000;
	static const unsigned int KERNEL_PAGE_POOL_SIZE = 0x200000 - 0x4000;

public:
	KernelPageManager(BitMapAllocator *allocator);
	int Initialize(); /* ��ʼ��MapNode map[0]Ϊ�ں�����ҳ����ʼ��ַ����С */
};

class UserPageManager : public PageManager
{
public:
	/* static const member */
	static const unsigned int USER_PAGE_POOL_START_ADDR = 0x400000; /* �û������ڴ�������ʼ��ַ */
	/* static member */
	static unsigned int USER_PAGE_POOL_SIZE; /* �û������ڴ������С�����ں˳�ʼ��ʱ�������� */

public:
	UserPageManager(BitMapAllocator *allocator);
	int Initialize(); /* ��ʼ��MapNode map[0]Ϊ�û�����ҳ����ʼ��ַ����С */
};

#endif
