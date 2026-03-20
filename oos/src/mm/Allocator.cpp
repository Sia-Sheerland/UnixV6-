#include "Allocator.h"

Allocator Allocator::m_Instance;

Allocator& Allocator::GetInstance()
{
	return Allocator::m_Instance;
}

unsigned long Allocator::Alloc(MapNode map[], unsigned long size)
{
	MapNode* pNode;
	unsigned long retIdx = 0;

	/* ��pNode->m_Size == 0���ʾ�Ѿ���������β */
	for ( pNode = map; pNode->m_Size; pNode++)
	{
		if ( pNode->m_Size >= size )
		{
			retIdx = pNode->m_AddressIdx;
			pNode->m_AddressIdx += size;
			pNode->m_Size -= size;
			/* ��ǰ�ڴ����÷�����ɣ�����MapNode����λ�ú����MapNode����ǰ�ƶ�һ��λ�� */
			if ( pNode->m_Size == 0 ) 
			{
				MapNode* pNextNode = (pNode + 1);
				for ( ; pNextNode->m_Size; ++pNode, ++pNextNode)
				{
					pNode->m_AddressIdx = pNextNode->m_AddressIdx;
					pNode->m_Size = pNextNode->m_Size;
				}
				pNode->m_AddressIdx = pNode->m_Size = 0;
			}
			return retIdx;
		}
	}
	/* no match found */
	return 0;
}

unsigned long Allocator::Free(MapNode map[], unsigned long size, unsigned long addrIdx)
{
	MapNode* pNode;
	/* ���ȣ�pNodeָ��������addrIdx��һ������� */
	for ( pNode = map; pNode->m_AddressIdx <= addrIdx && pNode->m_Size != 0; ++pNode );
	/* 
	 * 1) pNode���ǵ�һ�飬��������ͷ������
	 * 2) ��Ҫfree�����ݿ�������pLastNode����
	 * 3) ��Ϊ���pNode�ǵ�һ��������2)һ�����������
	 * ��˿��Ա�֤pLastNode��������һ������С��map�ĵ�ַ
	 */
	MapNode* pLastNode = pNode - 1;
	if ( pNode > map && addrIdx == pLastNode->m_AddressIdx + pLastNode->m_Size )
	{
		pLastNode->m_Size += size;
		/* ���ﴦ���������п����ڵ��������Ҫ�ϲ���������ǰ�ƶ����п� */
		if ( addrIdx + size == pNode->m_AddressIdx )  
		{
			pLastNode->m_Size += pNode->m_Size;
			for ( ++pLastNode, ++pNode; pNode->m_Size; ++pLastNode, ++pNode )
			{
				pLastNode->m_AddressIdx = pNode->m_AddressIdx;
				pLastNode->m_Size = pNode->m_Size;				
			}
			pLastNode->m_AddressIdx = pLastNode->m_Size = 0;
		}
	}
	/* ���ﴦ���������
	 * 1) ������pNode������pNode������Ч�ģ�ֻҪ�޸�pNode��AddressIdx���Լ���
	 * 2) ǰ�󶼲����ڣ�����Ҫ��pNode���Ժ�Ľڵ���������ƶ�
	 */
	else
	{
		if ( addrIdx + size == pNode->m_AddressIdx && pNode->m_Size )
		{
			pNode->m_AddressIdx = addrIdx;
			pNode->m_Size += size;
		}
		else if ( size ) //�Ϸ����ж�
		{
			//��������2)�����
			MapNode tmpNode1, tmpNode2;
			tmpNode1.m_AddressIdx = addrIdx;
			tmpNode1.m_Size = size;

			for ( ; pNode->m_Size; ++pNode )
			{
				tmpNode2.m_AddressIdx = pNode->m_AddressIdx;
				tmpNode2.m_Size = pNode->m_Size;

				pNode->m_AddressIdx = tmpNode1.m_AddressIdx;
				pNode->m_Size = tmpNode1.m_Size;

				tmpNode1.m_AddressIdx = tmpNode2.m_AddressIdx;
				tmpNode1.m_Size = tmpNode2.m_Size;
			}
			/* �����һ������pNode�� */
			pNode->m_AddressIdx = tmpNode1.m_AddressIdx;
			pNode->m_Size = tmpNode1.m_Size;
		}
	}
	return 0;
}

// NOTE:1
int BitMapAllocator::is_free(BitMap &bitmap, int index)
{
	return !(bitmap.map[index / 64] & (1ULL << (index % 64)));
}

void BitMapAllocator::set_bit(BitMap &bitmap, int index, int value)
{
	if (value)
		bitmap.map[index / 64] |= (1ULL << (index % 64));
	else
		bitmap.map[index / 64] &= ~(1ULL << (index % 64));
}

BitMapAllocator BitMapAllocator::m_Instance_bitmap;

BitMapAllocator &BitMapAllocator::GetInstance()
{
	return BitMapAllocator::m_Instance_bitmap;
}

unsigned long BitMapAllocator::Alloc(BitMap &bitmap, unsigned long size)
{
	unsigned long retIdx = 0;
	int pages_needed = (size + M_PAGE_SIZE - 1) / M_PAGE_SIZE;

	int start = -1, count = 0;
	for (int i = 0; i < (bitmap.rows * 64); i++)
	{
		if (is_free(bitmap, i))
		{
			if (count == 0)
				start = i;
			count++;
			if (count == pages_needed)
				break;
		}
		else
		{
			count = 0;
		}
	}

	if (count < pages_needed)
		return 0;

	for (int i = 0; i < pages_needed; i++)
	{
		set_bit(bitmap, start + i, 1);
	}

	retIdx = start * M_PAGE_SIZE + bitmap.m_AddressIdx;
	return retIdx;
}

unsigned long BitMapAllocator::Free(BitMap &bitmap, unsigned long size, unsigned long addrIdx)
{
	int start_page = (addrIdx - bitmap.m_AddressIdx) / M_PAGE_SIZE;
	int pages_to_free = (size + M_PAGE_SIZE - 1) / M_PAGE_SIZE;

	for (int i = 0; i < pages_to_free; i++)
	{
		set_bit(bitmap, start_page + i, 0);
	}
	return 0;
}

