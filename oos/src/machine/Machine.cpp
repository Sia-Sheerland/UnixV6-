#include "Machine.h"
#include "Assembly.h"
#include "Exception.h"
#include "TimeInterrupt.h"
#include "DiskInterrupt.h"
#include "KeyboardInterrupt.h"
#include "MouseInterrupt.h"
#include "SystemCall.h"

Machine Machine::instance;	/*��̬��ʵ���Ķ���*/

/* ȫ��GDT��IDT��TSS���� */
GDT g_GDT;
IDT g_IDT;

TaskStateSegment g_TaskStateSegment;

Machine& Machine::Instance()
{
	return instance;
}

void Machine::LoadIDT()
{
	IDTR idtr;
	GetIDT().FormIDTR(idtr);
	X86Assembly::LIDT((unsigned short*)(&idtr));
}

void Machine::LoadGDT()
{
	GDTR gdtr;
	GetGDT().FormGDTR(gdtr);
	X86Assembly::LGDT((unsigned short*)(&gdtr));
}

void Machine::LoadTaskRegister()
{
	X86Assembly::LTR(Machine::TASK_STATE_SEGMENT_SELECTOR);
}

extern "C" void MasterIRQ7();

void Machine::InitIDT()
{
	this->m_IDT = &g_IDT;
	/*
	 * 1. ��IDT��0 - 255������ȫ������Ĭ���ж�/�쳣����������ڣ�ȷ
	 *    ������һ���ж�/�쳣����ʱ���ᱻ�����������ں˱�����
	 * 2. ��INT 0 - 31���쳣��ʹ�÷�Ĭ�ϵ��ж�/�쳣�������򸲸���ǰ
	 *    Ĭ�ϴ���������ڡ�
	 * 3. ����ʱ���жϡ������жϡ������жϵȶ�Ӧ���ж���ڡ�
	 * 4. INT 0 - 31���쳣��ʹ��Ĭ�ϴ��������ģ�һ������²����ܷ�����
	 *    ������Щ�쳣�Ĵ������̲������ֳ�����ͻָ��������������Ϣ��
	 *    ������ѭ�����ȴ��˹���Ԥ��
	 */
	for ( int i = 0; i <= 255; i++ )
	{
		 if( i < 32 )
			 this->GetIDT().SetTrapGate(i, (unsigned long)IDT::DefaultExceptionHandler); 
		 else
			 this->GetIDT().SetInterruptGate(i, (unsigned long)IDT::DefaultInterruptHandler); 
	}
	/* ��ʼ��INT 0 - 31���쳣 */
	this->GetIDT().SetTrapGate(0, (unsigned long)Exception::DivideErrorEntrance);
	this->GetIDT().SetTrapGate(1, (unsigned long)Exception::DebugEntrance);
	this->GetIDT().SetTrapGate(2, (unsigned long)Exception::NMIEntrance);
	this->GetIDT().SetTrapGate(3, (unsigned long)Exception::BreakpointEntrance);
	this->GetIDT().SetTrapGate(4, (unsigned long)Exception::OverflowEntrance);
	this->GetIDT().SetTrapGate(5, (unsigned long)Exception::BoundEntrance);
	this->GetIDT().SetTrapGate(6, (unsigned long)Exception::InvalidOpcodeEntrance);
	this->GetIDT().SetTrapGate(7, (unsigned long)Exception::DeviceNotAvailableEntrance);
	this->GetIDT().SetTrapGate(8, (unsigned long)Exception::DoubleFaultEntrance);
	this->GetIDT().SetTrapGate(9, (unsigned long)Exception::CoprocessorSegmentOverrunEntrance);
	this->GetIDT().SetTrapGate(10,(unsigned long)Exception::InvalidTSSEntrance);
	this->GetIDT().SetTrapGate(11,(unsigned long)Exception::SegmentNotPresentEntrance);
	this->GetIDT().SetTrapGate(12,(unsigned long)Exception::StackSegmentErrorEntrance);
	this->GetIDT().SetTrapGate(13,(unsigned long)Exception::GeneralProtectionEntrance);
	
	/* ȱҳ�쳣(INT 14) UNIX V6++�ж���������ͼ���뻻������ҳʽ��������˲���Ҫȱҳ�쳣�������� */
	this->GetIDT().SetTrapGate(14,(unsigned long)Exception::PageFaultEntrance);
	/* Intel�����쳣(INT 15)  ʹ��IDT::DefaultExceptionHandler() */
	this->GetIDT().SetTrapGate(16,(unsigned long)Exception::CoprocessorErrorEntrance);
	this->GetIDT().SetTrapGate(17,(unsigned long)Exception::AlignmentCheckEntrance);
	this->GetIDT().SetTrapGate(18,(unsigned long)Exception::MachineCheckEntrance);
	this->GetIDT().SetTrapGate(19,(unsigned long)Exception::SIMDExceptionEntrance);

	/* INT 20 - 31���쳣ΪIntel����δʹ�õ��쳣 */

	/* ����ʱ���жϵ��ж��� */
	this->GetIDT().SetInterruptGate(0x20, (unsigned long)Time::TimeInterruptEntrance);
	/* ���ü����жϵ��ж��� */
	this->GetIDT().SetInterruptGate(0x21, (unsigned long)KeyboardInterrupt::KeyboardInterruptEntrance);
	/* ���������жϵ��ж��� (IRQ12) */
	this->GetIDT().SetInterruptGate(0x2C, (unsigned long)MouseInterrupt::MouseInterruptEntrance);
	/* ����IDT�д����ж϶�Ӧ�ж��� */
	this->GetIDT().SetInterruptGate(0x2E, (unsigned long)DiskInterrupt::DiskInterruptEntrance);
	/* 0x80���ж�������Ϊϵͳ���ã�����ϵͳ���ö�Ӧ�������� */
	this->GetIDT().SetTrapGate(0x80, (unsigned long)SystemCall::SystemCallEntrance);
	/* 8259A��Ƭ��irq7���Ż������δ֪�жϣ��ṩ�жϴ����������������� */
	this->GetIDT().SetInterruptGate(0x27, (unsigned long)MasterIRQ7);
}

void Machine::InitGDT()
{
	this->m_GDT = &g_GDT;
	
	//��ʼ��GDT�е�4���Σ��ں˴���Ρ��ں����ݶΣ��û�����Ρ��û����ݶ�
	//limit = 0xfffff, base = 0x00000000, G = 1 , D = 1(32bit), P =1, DPL = 00, S = 1, TYPE = 1010 (code segment read only) 
	//limit = 0xfffff, base = 0x00000000, G = 1, D = 1(32bit), P =1, DPL = 00, S = 1, TYPE = 0010 (data segment write/read) 
	//limit = 0xfffff, base = 0x00000000, G = 1 , D = 1(32bit), P =1, DPL = 11, S = 1, TYPE = 1010 (code segment read only) 
	//limit = 0xfffff, base = 0x00000000, G = 1, D = 1(32bit), P =1, DPL = 11, S = 1, TYPE = 0010 (data segment write/read) 
	
	//TODO ������Ӧ�Ŀɶ��ĳ�������GDTConsts::GRANULARITY_4K...
	SegmentDescriptor tmpDescriptor; 
	//0x08:00
	tmpDescriptor.SetSegmentLimit(0xfffff);
	tmpDescriptor.SetBaseAddress(0x00000000);
	tmpDescriptor.m_Granularity = 1;
	tmpDescriptor.m_DefaultOperationSize = 1;
	tmpDescriptor.m_SegmentPresent = 1;
	tmpDescriptor.m_DPL = 0x00;
	tmpDescriptor.m_System = 1;
	tmpDescriptor.m_Type = 0xA;	
	GetGDT().SetSegmentDescriptor(1, tmpDescriptor);

	//0x10:00
	tmpDescriptor.SetSegmentLimit(0xfffff);
	tmpDescriptor.SetBaseAddress(0x00000000);
	tmpDescriptor.m_Granularity = 1;
	tmpDescriptor.m_DefaultOperationSize = 1;
	tmpDescriptor.m_SegmentPresent = 1;
	tmpDescriptor.m_DPL = 0x00;
	tmpDescriptor.m_System = 1;
	tmpDescriptor.m_Type = 0x2;	
	GetGDT().SetSegmentDescriptor(2, tmpDescriptor);

	//0x18:00
	tmpDescriptor.SetSegmentLimit(0xfffff);
	tmpDescriptor.SetBaseAddress(0x00000000);
	tmpDescriptor.m_Granularity = 1;
	tmpDescriptor.m_DefaultOperationSize = 1;
	tmpDescriptor.m_SegmentPresent = 1;
	tmpDescriptor.m_DPL = 0x3;
	tmpDescriptor.m_System = 1;
	tmpDescriptor.m_Type = 0xA;	
	GetGDT().SetSegmentDescriptor(3, tmpDescriptor);

	//0x20:00
	tmpDescriptor.SetSegmentLimit(0xfffff);
	tmpDescriptor.SetBaseAddress(0x00000000);
	tmpDescriptor.m_Granularity = 1;
	tmpDescriptor.m_DefaultOperationSize = 1;
	tmpDescriptor.m_SegmentPresent = 1;
	tmpDescriptor.m_DPL = 0x3;
	tmpDescriptor.m_System = 1;
	tmpDescriptor.m_Type = 0x2;	
	GetGDT().SetSegmentDescriptor(4, tmpDescriptor);

	/* ��ʼ��TSS�� */
	this->m_TaskStateSegment = &g_TaskStateSegment;
	this->InitTaskStateSegment();
}


void Machine::InitPageDirectory()
{
	/* 
	 * ʵ�ֲ���ϵͳ��ҳ��ӳ��:
	 * �����ڴ�0x00000000-0x00400000(0-4M)����ӳ�䵽���Ե�ַ
	 * 0x00000000-0x00400000 �� 0xC0000000-0xC0400000
	 */
	PageDirectory* pPageDirectory = (PageDirectory*)(PAGE_DIRECTORY_BASE_ADDRESS + KERNEL_SPACE_START_ADDRESS);
	
	/* ��дҳĿ¼��0x200#ҳ�����ĵ�0�ʹ���Ե�ַ0-4Mӳ�䵽�����ڴ�0-4M */
	/*
	pPageDirectory->m_Entrys[0].m_UserSupervisor = 1;                   //�û�̬
	pPageDirectory->m_Entrys[0].m_Present = 1;
	pPageDirectory->m_Entrys[0].m_ReadWriter = 1;
	pPageDirectory->m_Entrys[0].m_PageTableBaseAddress = KERNEL_PAGE_TABLE_BASE_ADDRESS >> 12;
	*/

	/* ��дҳĿ¼��0x200#��ҳ���ĵ�768�ʹ���Ե�ַ0xC0000000-0xC0400000ӳ�䵽�����ڴ�0-4M��δ������̬�ռ�ߴ����4M�ֽڣ��ǵ�����Ҫ��*/
	unsigned int kPageTableIdx = KERNEL_SPACE_START_ADDRESS / PageTable::SIZE_PER_PAGETABLE_MAP; 
	pPageDirectory->m_Entrys[kPageTableIdx].m_UserSupervisor = 0;       // ����̬
	pPageDirectory->m_Entrys[kPageTableIdx].m_Present = 1;
	pPageDirectory->m_Entrys[kPageTableIdx].m_ReadWriter = 1;
	pPageDirectory->m_Entrys[kPageTableIdx].m_PageTableBaseAddress = KERNEL_PAGE_TABLE_BASE_ADDRESS >> 12;

	/* 
	 * ��ʼ������̬ҳ��������̬ҳ���������������ַ
	 * 0x200000(2M)������Ӧ���Ե�ַ��Ϊ0xC0200000
	 */
	PageTable* pPageTable = (PageTable*)(KERNEL_PAGE_TABLE_BASE_ADDRESS + KERNEL_SPACE_START_ADDRESS);
	/* 
	 * ʹ�������ڴ�0-4M��дҳ���ı��������������ڴ�0-4M
	 * ӳ�䵽��λ0xC0000000-0xC0400000��������ϵͳ�ں�ʹ�á�
	 */
	for ( unsigned int i = 0; i < PageTable::ENTRY_CNT_PER_PAGETABLE; i++ )
	{
		pPageTable->m_Entrys[i].m_UserSupervisor = 0;
		pPageTable->m_Entrys[i].m_Present = 1;
		pPageTable->m_Entrys[i].m_ReadWriter = 1;
		pPageTable->m_Entrys[i].m_PageBaseAddress = i;
	}

	this->m_PageDirectory = pPageDirectory;
	this->m_KernelPageTable = pPageTable;	
}

void Machine::InitUserPageTable()
{
	PageDirectory* pPageDirectory = this->m_PageDirectory;
	PageTable* pUserPageTable = 
		(PageTable*)(USER_PAGE_TABLE_BASE_ADDRESS + KERNEL_SPACE_START_ADDRESS);
	unsigned int idx = USER_PAGE_TABLE_BASE_ADDRESS >> 12;
	
	for ( unsigned int j = 0; j < USER_PAGE_TABLE_CNT; j++, idx++ )
	{
		pPageDirectory->m_Entrys[j].m_UserSupervisor = 1;
		pPageDirectory->m_Entrys[j].m_Present = 1;
		pPageDirectory->m_Entrys[j].m_ReadWriter = 1;
		/* 
		 * ҳĿ¼��BaseAddress�ֶ��м�¼ҳ����������ʼ��ַ���������Ե�ַ��
		 * Ҳ����˵����ҳ�����о���ҳĿ¼��BaseAddress�ֶ�����һ��ҳ����
		 * ����ҳ����������ַ�ҵ�������ҳ���Ƶ�������������ҳ���Ƶı���--�����Ե�ַ�Ľ�����
		 */
		pPageDirectory->m_Entrys[j].m_PageTableBaseAddress = idx;
		
		for ( unsigned int i = 0; i < PageTable::ENTRY_CNT_PER_PAGETABLE; i++ )
		{
			pUserPageTable[j].m_Entrys[i].m_UserSupervisor = 1;
			pUserPageTable[j].m_Entrys[i].m_Present = 1;
			pUserPageTable[j].m_Entrys[i].m_ReadWriter = 1;
			pUserPageTable[j].m_Entrys[i].m_PageBaseAddress = 0x00000 + i +j * 1024;
		}
	}

	this->m_UserPageTable = pUserPageTable;	
}

void Machine::InitTaskStateSegment()
{
	TaskStateSegment& tss = this->GetTaskStateSegment();
	tss.m_CR3 = 0x200000;	/* Physical base address of page directory */
	tss.m_CS = Machine::USER_CODE_SEGMENT_SELECTOR;
	tss.m_DS = Machine::USER_DATA_SEGMENT_SELECTOR;
	tss.m_SS = Machine::USER_DATA_SEGMENT_SELECTOR;
	tss.m_ES = tss.m_FS = tss.m_GS = Machine::USER_DATA_SEGMENT_SELECTOR;

	tss.m_EBP = 0xC0400000;
	tss.m_ESP = 0xC0400000;
	tss.m_EIP = 0xC0000000;	//runtime
	tss.m_EFLAGS = 0x200;	/* ����enable IF λ */
	tss.m_SS0 = Machine::KERNEL_DATA_SEGMENT_SELECTOR;
	tss.m_ESP0 = 0xC0400000;	/* ����̬��ַ�ռ�ĩβ��Ϊջ�� */

	/* 
	 * ��GDT���ĵ�5��(Machine::TASK_STATE_SEGMENT_IDX)ָ��TSS�Ρ�
	 * 
	 * ����GDT�������SegmentDescriptor���飬����û�ж�TSS��
	 * �������ĳ��������Ҫ��TaskStateSegmentDescriptorǿת��
	 * �Խ��б�Ҫ�����á�
	 */
	TaskStateSegmentDescriptor* p_TSSDescriptor = 
		(TaskStateSegmentDescriptor*)(&(GetGDT().GetSegmentDescriptor(Machine::TASK_STATE_SEGMENT_IDX)));
	p_TSSDescriptor->SetSegmengLimit(0x68 - 1);
	p_TSSDescriptor->SetBaseAddress((unsigned long)&g_TaskStateSegment);
	p_TSSDescriptor->m_Granularity = 1;
	p_TSSDescriptor->m_Type = 0x9; //����λΪbusyλ������Ϊ0
	p_TSSDescriptor->m_Present = 0x1;
	p_TSSDescriptor->m_Available = 0x1;
	p_TSSDescriptor->m_DescriptorPrivilegeLevel = 0x00;
}
void Machine::EnablePageProtection()
{
	/* 
	 * pageDirBaseAddr���ڸ�λ�ں˿ռ�����Ե�ַ����Ҫת��Ϊ������ַ��
	 * PhysicalAddress = LinearAddress - 0xC0000000
	 */
	unsigned int pageDirBaseAddr = (unsigned int)(&GetPageDirectory());
	unsigned int pageDirPhyBaseAddr = pageDirBaseAddr - Machine::KERNEL_SPACE_START_ADDRESS;
	
	/* �Ĵ���CR3��д��ҳĿ¼��ʼ������ַ��CR0��PGλ��1��������ҳ���� */
	__asm__ __volatile__("	movl %0, %%cr3;		\
							movl %%cr0, %%eax;	\
							orl $0x80000000, %%eax;	\
							movl %%eax, %%cr0" : : "a"(pageDirPhyBaseAddr) );
}

IDT& Machine::GetIDT()
{
	return *(this->m_IDT);
}

GDT& Machine::GetGDT()
{
	return *(this->m_GDT);
}

PageDirectory& Machine::GetPageDirectory()
{
	return *(this->m_PageDirectory);
}

PageTable& Machine::GetKernelPageTable()
{
	return *(this->m_KernelPageTable);
}

PageTable* Machine::GetUserPageTableArray()
{
	return this->m_UserPageTable;
}

TaskStateSegment& Machine::GetTaskStateSegment()
{
	return *(this->m_TaskStateSegment);
}


