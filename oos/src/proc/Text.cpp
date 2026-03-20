#include "Text.h"
#include "Kernel.h"

Text::Text()
{
	// nothing to do here
}

Text::~Text()
{
	// nothing to do here
}

void Text::XccDec()
{
	User &u = Kernel::Instance().GetUser();
	PageTable *pUserPageTable = u.u_MemoryDescriptor.m_UserPageTableArray;
	UserPageManager &userPageMgr = Kernel::Instance().GetUserPageManager();

	if (this->x_ccount == 0)
		return;

	--this->x_ccount;  	// ���x_ccount�ݼ���0���ͷŹ������Ķ�ռ�ݵ��ڴ�

	int index = u.u_MemoryDescriptor.m_DataStartAddress >> 12 - 1024;
	int count = (u.u_MemoryDescriptor.m_DataSize + PageManager::PAGE_SIZE - 1) / PageManager::PAGE_SIZE;
	int frame;
	while (count)
	{
		if (this->x_ccount == 0)
		{
			frame = pUserPageTable->m_Entrys[index].m_PageBaseAddress;  // �ͷ�����ҳ��
			userPageMgr.FreeMemory(PageManager::PAGE_SIZE, frame << 12);
		}
		pUserPageTable->m_Entrys[index].m_Present = 0;				// ���PTE
		pUserPageTable->m_Entrys[index].m_ReadWriter = 0;
		pUserPageTable->m_Entrys[index].m_UserSupervisor = 1;
		pUserPageTable->m_Entrys[index].m_PageBaseAddress = 0;

		index++;		count--;
	}
}

void Text::XFree()
{
	/* �ͷ��ڴ��еĸ��� */
	this->XccDec();

	/* �ͷ��̽������ϵĸ��� */
	if (--this->x_count == 0)
	{
		Kernel::Instance().GetSwapperManager().FreeSwap(this->x_size, this->x_daddr);
		Kernel::Instance().GetFileManager().m_InodeTable->IPut(this->x_iptr);
		this->x_iptr = NULL;
	}
}
