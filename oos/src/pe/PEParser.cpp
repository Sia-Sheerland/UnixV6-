#include "PEParser.h"
#include "Utility.h"
#include "PageManager.h"
#include "MemoryDescriptor.h"
#include "User.h"
#include "Kernel.h"
#include "Machine.h"

PEParser::PEParser()
{
    this->EntryPointAddress = 0;
    this->sectionHeaders = 0;
}

/* ԭ��V6++��PEParser */
PEParser::PEParser(unsigned long peAddress)
{
	this->peAddress = peAddress + 0xC0000000;   // peͷ�����ַ
}

unsigned int PEParser::Relocate(Inode* p_inode, int sharedText)
{
	User& u = Kernel::Instance().GetUser();
	unsigned long srcAddress, desAddress;
	unsigned cnt = 0;
	unsigned int i = 0;
	unsigned int i0 = 0;

	/* ������Ժ��������̹������ĶΣ������ļ��ж������Ķ� */
	PageTable* pUserPageTable = Machine::Instance().GetUserPageTableArray();
	unsigned int textBegin = this->TextAddress >> 12 , textLength = this->TextSize >> 12;
	PageTableEntry* pointer = (PageTableEntry *)pUserPageTable;

	/*������������̹������ĶΣ��������Ķ��в�����0*/
	if(sharedText == 1)
		i = 1;      // i�Ƕ�ͷ����
	else
	{
		i = 0;
		// �޸����ĶεĶ�д��־��Ϊ�ں�д�������׼��
		for (i0 = textBegin; i0 < textBegin + textLength; i0++)
			pointer[i0].m_ReadWriter = 1;

		FlushPageDirectory();
	}

    /* ������ҳ��ִ����0����������bss�����ĳ�ֵ����0 */
	for (; i <= this->BSS_SECTION_IDX; i++ )
	{
		ImageSectionHeader* sectionHeader = &(this->sectionHeaders[i]);
		int beginVM = sectionHeader->VirtualAddress + ntHeader.OptionalHeader.ImageBase;
		int size = ((sectionHeader->Misc.VirtualSize + PageManager::PAGE_SIZE - 1)>>12)<<12;
		int j;

		for (j=0; j<size; j++)
		{
			unsigned char* b =(unsigned char*)(j + beginVM);
			*b = 0;
		}
	}

	/* �����ĶΣ�optional�������ļ�����ȫ�ֱ����ĳ�ֵ  */
 	if(sharedText == 1)
		i = 1;      // i�Ƕ�ͷ����
	else
	// �޸����ĶεĶ�д��־��Ϊ�ں�д�������׼��
		i = 0;

	for ( ; i < this->BSS_SECTION_IDX; i++ )
	{
		ImageSectionHeader* sectionHeader = &(this->sectionHeaders[i]);
		srcAddress = sectionHeader->PointerToRawData;
		desAddress =
			this->ntHeader.OptionalHeader.ImageBase + sectionHeader->VirtualAddress;

	    u.u_IOParam.m_Base = (unsigned char*)desAddress;
	    u.u_IOParam.m_Offset = srcAddress;
	    u.u_IOParam.m_Count = sectionHeader->Misc.VirtualSize;

	    p_inode->ReadI();

		cnt += sectionHeader->Misc.VirtualSize;
	}

	if(sharedText == 0)
	{   //�����Ķ�ҳ��Ļ�ֻ��
		for (i0 = textBegin; i0 < textBegin + textLength; i0++)
			pointer[i0].m_ReadWriter = 0;

		FlushPageDirectory();
	}

	KernelPageManager& kpm = Kernel::Instance().GetKernelPageManager();
	kpm.FreeMemory(PageManager::PAGE_SIZE * 2, (unsigned long)this->sectionHeaders - 0xC0000000 );
//	kpm.FreeMemory(section_size * ntHeader.FileHeader.NumberOfSections, (unsigned long)this->sectionHeaders - 0xC0000000 );
//	delete this->sectionHeaders;
	return 	cnt;
}

/* ԭ��V6++ʹ�õĴ��룬�ַ��������� */
unsigned int PEParser::Relocate()
{
	unsigned long srcAddress, desAddress;
	unsigned cnt = 0;

	for (unsigned int i = 0; i < this->BSS_SECTION_IDX; i++ )
	{
		ImageSectionHeader* sectionHeader = &(this->sectionHeaders[i]);
		srcAddress = this->peAddress + sectionHeader->PointerToRawData;
		desAddress = 
			this->ntHeader.OptionalHeader.ImageBase + sectionHeader->VirtualAddress;
		Utility::MemCopy(srcAddress, desAddress, sectionHeader->Misc.VirtualSize);
		cnt += sectionHeader->Misc.VirtualSize;
	}

	return 	cnt;
}

bool PEParser::HeaderLoad(Inode* p_inode)
{
    ImageDosHeader dos_header;
    User& u = Kernel::Instance().GetUser();
    KernelPageManager& kpm = Kernel::Instance().GetKernelPageManager();

    /*��ȡdos header*/
    u.u_IOParam.m_Base = (unsigned char*)&dos_header;
    u.u_IOParam.m_Offset = 0;
    u.u_IOParam.m_Count = 0x40;
    p_inode->ReadI();

    /*��ȡnt_Header*/
    u.u_IOParam.m_Base = (unsigned char*)(&this->ntHeader);
    u.u_IOParam.m_Offset = dos_header.e_lfanew;
    u.u_IOParam.m_Count = ntHeader_size;
    p_inode->ReadI();

    if ( ntHeader.Signature!=0x00004550 )
	{
        return false;
	}

    sectionHeaders = (ImageSectionHeader*)(kpm.AllocMemory(PageManager::PAGE_SIZE * 2) + 0xC0000000);
    u.u_IOParam.m_Base = (unsigned char*)sectionHeaders;
    u.u_IOParam.m_Offset = dos_header.e_lfanew + ntHeader_size;
    u.u_IOParam.m_Count = section_size * ntHeader.FileHeader.NumberOfSections;
    p_inode->ReadI();

    /*
    	 * @comment ����hardcode gcc���߼�
    	 * section ˳��Ϊ .text->.data->.rdata->.bss
    	 *
    */
	this->TextAddress =
		ntHeader.OptionalHeader.BaseOfCode + ntHeader.OptionalHeader.ImageBase;
	this->TextSize =
		ntHeader.OptionalHeader.BaseOfData - ntHeader.OptionalHeader.BaseOfCode;

	this->DataAddress =
		ntHeader.OptionalHeader.BaseOfData + ntHeader.OptionalHeader.ImageBase;
	this->DataSize = this->sectionHeaders[this->IDATA_SECTION_IDX].VirtualAddress - ntHeader.OptionalHeader.BaseOfData;

    StackSize = ntHeader.OptionalHeader.SizeOfStackCommit;
    HeapSize = ntHeader.OptionalHeader.SizeOfHeapCommit;

    EntryPointAddress = ntHeader.OptionalHeader.AddressOfEntryPoint +
                    ntHeader.OptionalHeader.ImageBase;

    /* 记录各节的文件偏移和虚拟地址，供缺页调页使用 */
    NumSections = ntHeader.FileHeader.NumberOfSections;
    if (NumSections > MAX_SECTIONS) NumSections = MAX_SECTIONS;
    for (unsigned int s = 0; s < NumSections; s++) {
        SectionFileOffset[s] = sectionHeaders[s].PointerToRawData;
        SectionVirtAddr[s]   = sectionHeaders[s].VirtualAddress
                               + ntHeader.OptionalHeader.ImageBase;
        SectionVirtSize[s]   = sectionHeaders[s].Misc.VirtualSize;
        SectionRawSize[s]    = sectionHeaders[s].SizeOfRawData;
    }

	return true;
}
