#include "Exception.h"
#include "Kernel.h"
#include "Utility.h"
#include "Video.h"
#include "Machine.h"
#include "VmAreaStruct.h"
#include "PageRefCount.h"
#include "MemoryDescriptor.h"
#include "PageManager.h"
#include "File.h"

/* 
 * ����INT 0 - INT 31���쳣��IDT�е���ں���(Entrance)
 * -->�޳�����<-- ���쳣
 */
#define IMPLEMENT_EXCEPTION_ENTRANCE(Exception_Entrance, Exception_Handler) \
void Exception::Exception_Entrance() \
{ \
	SaveContext();			\
							\
	SwitchToKernel();		\
							\
	CallHandler(Exception, Exception_Handler);	\
							\
	RestoreContext();		\
							\
	Leave();				\
							\
	InterruptReturn();		\
}

/* 
 * ����INT 0 - INT 31���쳣��IDT�е���ں���(Entrance)
 * -->�г�����(ErrCode)<-- ���쳣
 * ���ڳ����������iret�жϷ���ָ��֮ǰ�ֶ���ջ�ϵ�����
 * �����г����������£���leaveָ������ջ֡��������
 * ջ�ϵ�4���ֽڳ����롣
 */
#define IMPLEMENT_EXCEPTION_ENTRANCE_ERRCODE(Exception_Entrance, Exception_Handler) \
void Exception::Exception_Entrance() \
{ \
	SaveContext();			\
							\
	SwitchToKernel();		\
							\
	CallHandler(Exception, Exception_Handler);	\
							\
	RestoreContext();		\
							\
	Leave();				\
							\
	__asm__ __volatile__("addl $4, %%esp" ::);	\
							\
	InterruptReturn();		\
}

/*
	=========================
	������������һ��˵����
	=========================
	Ŀǰ������������IMPLEMENT_EXCEPTION_ENTRANCE_ERRCODE��IMPLEMENT_EXCEPTION_ENTRANCE�в��õĶ���
	Inline Assembly��û�������κ���ʱ���ֲ�����(��int i, j;֮������)���ߺ������ã�������ں���(Entrance)�г�����ջ��ѹ
	��ebp֮�⣬û����ջ�Ϸ����κζ�����ֽڣ�����Ϳ��ܳ�������������
	
	EFLAGS
	CS
	EIP
	[ERRORCODE]	//Optional
	ebp
	xx�ֽڿռ�  	<--����ʱ���ֲ�����ռ�õĶ�ջ�ռ䡰  
	SaveContext();
	
	��ᵼ������������ĳһ���ṹ�壬��pt_regs������ͨ�üĴ���(eax,ebx...��)��ERRORCODE��eip��cs��eflags��ȫ���ֶΣ�
	��ô��SaveContext()�����ֶκ�ERRORCODE��eip��cs��eflags֮���϶Ӧ��Ԥ�������ֽڵ�����ֶ�(padding)���ṹ����Ӧ��
	Ԥ�������ֽڳ��ȵ�����ֶ����޷�Ԥ�ȼ���õ��ģ����ҳ��Ȼ����ź����������ֲ��������ٶ��ɱ������Զ�ȷ����
	���ǲ�û��ֻ����һ���ṹ��pt_regs������ȫ�����ֶΣ����ǲ�����pt_regs��pt_context�����ֶΣ�pt_regs������ͨ�üĴ���
	�е��ֳ���Ϣ����pt_context��������ж���ָ�����ֳ�(eflags��cs��eip��[ERRORCODE])�����⣬��SaveContext()���ʵ
	���У�����

	=========================
	����leaveָ���һ��˵����
	=========================
	������X86Assembly����ʵ�ֶ�leave��iretָ���װ�ĺ�����
	��leave��iret��2��ָ����к�����װ���ڵ���ʱ�����һЩ���⣬���������**Entrance()������ֱ��ʹ�ú��װ��������ࡣ	
	X86Assembly::Leave()�����ķ�������������£�
	
	push   %ebp
	mov    %esp,%ebp
	leave  
	pop    %ebp
	ret

	leaveָ��ȼ���2��ָ��: mov %ebp, %esp; pop ebp; �����������ٵ�ǰ�������õ�ջ֡������X86Assembly::Leave()�����е�leave
	ָ�����ٵ������ǵ���X86Assembly::Leave()������ջ֡���Ⲣ�����ǵı��⣬������RestoreContext();֮��ʹ��leaveָ���Ŀ���ǣ�
	��λ��ջ������ebpʱ(���쳣��ں���(Entrance)�������ɵĵ�һ��ָ��ѹ���ebp)����leaveָ��ʹ��ebp��ջ�е����Լ��ָ�esp���Ӷ�����
	�쳣��ں���(Entrance)��ջ֡��
	�����ĿǰΨһ�Ľ���취��ֱ����Inline Assembly��������leaveָ����к�����װ��
	
	=========================
	����iretָ���һ��˵����
	=========================
	������Ҫ�ڵ�ջ����ŵ�Ԫ��������:
	SS
	ESP
	EFLAGS
	CS
	EIP	<--  ��ǰջ��λ��
	
	�ſ���ʹ��iretָ����жϷ��أ�����iretָ����з�װX86Assembly::IRet()�Ľ�����ǣ���ջ������£�
	SS
	ESP
	EFLAGS
	CS
	EIP	
	ebp	<--  ��ǰջ��λ�ã� ebp��X86Assembly::IRet()������һ��ָ��ѹ���
	����ebpΪջ��������½���iret���ᷢ�����ش��󣬴���ذ�ebp����EIP, EIP����CS��ִ��iretָ�����ϵͳ������
*/


/* 
 * ����INT 0 - INT 31���쳣��������(Handler)��2���ꡣ
 * 
 * (1)	IMPLEMENT_EXCEPTION_HANDLER_ERRCODE(Exception_Handler, Error_Message, Signal_Value)
 * ��Ӧ�г������������ڶ�������ʹ��struct pte_context* context;
 * 
 * (2)	IMPLEMENT_EXCEPTION_HANDLER(Exception_Handler, Error_Message, Signal_Value)
 * ��Ӧ�޳������������ڶ�������ʹ��struct pt_context* context;
 * 
 * ���������������ڵڶ������ǰ���error_code�Ľṹ��pte_context, ����û��
 * error_code�ֶεĽṹ��pt_context!
 */

#define IMPLEMENT_EXCEPTION_HANDLER(Exception_Handler, Error_Message, Signal_Value) \
void Exception::Exception_Handler(struct pt_regs* regs, struct pt_context* context) \
{	\
	User& u = Kernel::Instance().GetUser();			\
	Process* current = u.u_procp;					\
													\
	if ( (context->xcs & USER_MODE) == USER_MODE )	\
	{												\
		current->PSignal(Signal_Value);				\
		if ( current->IsSig() )						\
			current->PSig(context);					\
	}												\
	else											\
	{												\
		Utility::Panic(Error_Message);				\
	}												\
}

#define IMPLEMENT_EXCEPTION_HANDLER_ERRCODE(Exception_Handler, Error_Message, Signal_Value) \
void Exception::Exception_Handler(struct pt_regs* regs, struct pte_context* context) \
{	\
	User& u = Kernel::Instance().GetUser();			\
	Process* current = u.u_procp;					\
													\
	if ( (context->xcs & USER_MODE) == USER_MODE )	\
	{												\
		current->PSignal(Signal_Value);				\
		if ( current->IsSig() )						\
			current->PSig( (pt_context *)&context->eip );		\
	}												\
	else											\
	{												\
		Utility::Panic(Error_Message);				\
	}												\
}


Exception::Exception()
{
	//NOTHING IS OK
}

Exception::~Exception()
{
	//NOTHING IS OK
}


//�����(INT 0)
IMPLEMENT_EXCEPTION_ENTRANCE(DivideErrorEntrance, DivideError)
IMPLEMENT_EXCEPTION_HANDLER(DivideError, "Divide Exception!", User::SIGFPE)


//�����쳣(INT 1)
IMPLEMENT_EXCEPTION_ENTRANCE(DebugEntrance, Debug)
IMPLEMENT_EXCEPTION_HANDLER(Debug, "Debug Exception!", User::SIGTRAP)


//NMI�������ж�(INT 2)
IMPLEMENT_EXCEPTION_ENTRANCE(NMIEntrance, NMI)
IMPLEMENT_EXCEPTION_HANDLER(NMI, "Non-maskable Interrupt!", User::SIGNUL)


//���Զϵ�(INT 3)
IMPLEMENT_EXCEPTION_ENTRANCE(BreakpointEntrance, Breakpoint)
IMPLEMENT_EXCEPTION_HANDLER(Breakpoint, "Breakpoint Exception!", User::SIGTRAP)


//���(INT 4)
IMPLEMENT_EXCEPTION_ENTRANCE(OverflowEntrance, Overflow)
IMPLEMENT_EXCEPTION_HANDLER(Overflow, "Overflow Exception!", User::SIGSEGV)


//BOUNDָ���쳣(INT 5)
IMPLEMENT_EXCEPTION_ENTRANCE(BoundEntrance, Bound)
IMPLEMENT_EXCEPTION_HANDLER(Bound, "Bound Range Exceeded!", User::SIGSEGV)


//��Ч������(INT 6)
IMPLEMENT_EXCEPTION_ENTRANCE(InvalidOpcodeEntrance, InvalidOpcode)
IMPLEMENT_EXCEPTION_HANDLER(InvalidOpcode, "Invalid Opcode!", User::SIGILL)


//�豸������(INT 7)
IMPLEMENT_EXCEPTION_ENTRANCE(DeviceNotAvailableEntrance, DeviceNotAvailable)
IMPLEMENT_EXCEPTION_HANDLER(DeviceNotAvailable, "Device Not Available!", User::SIGSEGV)


//˫�ش���(INT 8)  *�г�����*
IMPLEMENT_EXCEPTION_ENTRANCE_ERRCODE(DoubleFaultEntrance, DoubleFault)
IMPLEMENT_EXCEPTION_HANDLER_ERRCODE(DoubleFault, "Double Fault Exception!", User::SIGSEGV)


//Э��������Խ��(INT 9)
IMPLEMENT_EXCEPTION_ENTRANCE(CoprocessorSegmentOverrunEntrance, CoprocessorSegmentOverrun)
IMPLEMENT_EXCEPTION_HANDLER(CoprocessorSegmentOverrun, "Coprocessor Segment Overrun!", User::SIGFPE)


//��ЧTSS(INT 10)  *�г�����*
IMPLEMENT_EXCEPTION_ENTRANCE_ERRCODE(InvalidTSSEntrance, InvalidTSS)
IMPLEMENT_EXCEPTION_HANDLER_ERRCODE(InvalidTSS, "Invalid TSS!", User::SIGSEGV)


//�β�����(INT 11)  *�г�����*
IMPLEMENT_EXCEPTION_ENTRANCE_ERRCODE(SegmentNotPresentEntrance, SegmentNotPresent)
IMPLEMENT_EXCEPTION_HANDLER_ERRCODE(SegmentNotPresent, "Segment Not Present!", User::SIGBUS)


//��ջ�δ���(INT 12)  *�г�����*
IMPLEMENT_EXCEPTION_ENTRANCE_ERRCODE(StackSegmentErrorEntrance, StackSegmentError)
IMPLEMENT_EXCEPTION_HANDLER_ERRCODE(StackSegmentError, "Stack Segment Error!", User::SIGBUS)


//һ�㱣�����쳣(INT 13)  *�г�����*
IMPLEMENT_EXCEPTION_ENTRANCE_ERRCODE(GeneralProtectionEntrance, GeneralProtection)
IMPLEMENT_EXCEPTION_HANDLER_ERRCODE(GeneralProtection, "General Protection!", User::SIGSEGV)



//ȱҳ�쳣(INT 14)  *�г�����*
IMPLEMENT_EXCEPTION_ENTRANCE_ERRCODE(PageFaultEntrance, PageFault)
//IMPLEMENT_EXCEPTION_HANDLER_ERRCODE(PageFault, "Page Fault!", User::SIGSEGV)

void Exception::PageFault(struct pt_regs* regs, struct pte_context* context)
{
	User&             u       = Kernel::Instance().GetUser();
	Process*          current = u.u_procp;
	MemoryDescriptor& md      = u.u_MemoryDescriptor;

	unsigned int cr2;
	__asm__ __volatile__("mov %%cr2, %0" : "=r"(cr2));

	/* error_code 由硬件压栈，通过 context->error_code 取得 */
	bool isWrite = (context->error_code & 0x2) != 0;

	/* 只处理用户态缺页；内核态缺页直接 panic */
	if ((context->xcs & USER_MODE) != USER_MODE) {
		Diagnose::Write("Kernel PageFault CR2=%x EIP=%x\n", cr2, context->eip);
		Utility::Panic("Page Fault in Kernel Mode.");
		return;
	}

	unsigned long pageVA = (unsigned long)cr2 & ~0xFFFu;

	/* ① 查找 VMA */
	VmAreaStruct* vma = md.FindVma(cr2);

	if (!vma) {
		/* 检查是否为栈扩展 */
		unsigned long stackBase = MemoryDescriptor::USER_SPACE_SIZE - md.m_StackSize;
		if (cr2 < stackBase && cr2 >= stackBase - PageManager::PAGE_SIZE
		    && md.m_DataSize + md.m_StackSize + PageManager::PAGE_SIZE
		       < MemoryDescriptor::USER_SPACE_SIZE - md.m_DataStartAddress) {
			current->SStack();
			return;
		}
		Diagnose::Write("SIGSEGV: no VMA for %x\n", cr2);
		current->PSignal(User::SIGSEGV);
		if (current->IsSig())
			current->PSig((pt_context*)&context->eip);
		return;
	}

	/* 保护检查：向只读区写 */
	if (isWrite && !(vma->vm_prot & VM_PROT_WRITE)) {
		Diagnose::Write("SIGSEGV: write to R/O VMA at %x\n", cr2);
		current->PSignal(User::SIGSEGV);
		if (current->IsSig())
			current->PSig((pt_context*)&context->eip);
		return;
	}

	PageTableEntry& spte = md.GetPTE(pageVA);
	UserPageManager& upm = Kernel::Instance().GetUserPageManager();

	/* ② 情形 A：PTE 为空（首次访问） */
	if (!spte.m_Present && spte.m_PageBaseAddress == 0) {
		unsigned long physPage = upm.AllocMemory(PageManager::PAGE_SIZE);
		if (!physPage) {
			Diagnose::Write("PageFault OOM\n");
			current->PSignal(User::SIGSEGV);
			if (current->IsSig())
				current->PSig((pt_context*)&context->eip);
			return;
		}
		PageRefCount::Set(physPage, 1);

		bool writable = (vma->vm_prot & VM_PROT_WRITE) != 0;
		/* 先映射，再通过用户虚拟地址清零/读文件 */
		md.MapPageDirect(pageVA, physPage >> 12, writable);

		/* 清零 */
		unsigned char* uva = (unsigned char*)pageVA;
		for (int i = 0; i < (int)PageManager::PAGE_SIZE; i++) uva[i] = 0;

		/* 文件支撑的 VMA：从文件读入数据 */
		if (vma->vm_inode && vma->f_len > 0) {
			unsigned long offInVma = pageVA - vma->vm_start;
			if (offInVma < vma->f_len) {
				unsigned long toRead = vma->f_len - offInVma;
				if (toRead > PageManager::PAGE_SIZE)
					toRead = PageManager::PAGE_SIZE;
				IOParameter saved = u.u_IOParam;
				u.u_IOParam.m_Base   = uva;
				u.u_IOParam.m_Offset = (int)(vma->f_start + offInVma);
				u.u_IOParam.m_Count  = (int)toRead;
				vma->vm_inode->ReadI();
				u.u_IOParam = saved;
			}
		}
		return;
	}

	/* ③ 情形 B：P=0 但 PageBaseAddress 非零（换出到 Swap） */
	if (!spte.m_Present && spte.m_PageBaseAddress != 0) {
		unsigned long physPage = upm.AllocMemory(PageManager::PAGE_SIZE);
		if (!physPage) {
			current->PSignal(User::SIGSEGV);
			if (current->IsSig())
				current->PSig((pt_context*)&context->eip);
			return;
		}
		PageRefCount::Set(physPage, 1);
		int blkno = (int)spte.m_PageBaseAddress;
		bool writable = (vma->vm_prot & VM_PROT_WRITE) != 0;
		md.MapPageDirect(pageVA, physPage >> 12, writable);
		Kernel::Instance().GetBufferManager().Swap(
			blkno, physPage, PageManager::PAGE_SIZE, Buf::B_READ);
		Kernel::Instance().GetSwapperManager().FreeSwap(
			PageManager::PAGE_SIZE, blkno);
		return;
	}

	/* ④ 情形 C：写时复制（P=1 但 R/O，写操作触发） */
	if (spte.m_Present && isWrite && !spte.m_ReadWriter) {
		unsigned long oldPhys = (unsigned long)spte.m_PageBaseAddress << 12;
		unsigned char refCnt  = PageRefCount::Get(oldPhys);

		if (refCnt > 1) {
			/* 共享页：分配新页并复制内容 */
			unsigned long newPage = upm.AllocMemory(PageManager::PAGE_SIZE);
			if (!newPage) {
				current->PSignal(User::SIGSEGV);
				if (current->IsSig())
					current->PSig((pt_context*)&context->eip);
				return;
			}
			PageRefCount::Set(newPage, 1);
			PageRefCount::Dec(oldPhys);
			/* 使用借用内核页表项复制物理页内容 */
			PageTableEntry* kpt = Machine::Instance().GetKernelPageTable().m_Entrys;
			unsigned long f1 = kpt[258].m_PageBaseAddress;
			unsigned long f2 = kpt[259].m_PageBaseAddress;
			kpt[258].m_PageBaseAddress = oldPhys >> 12;
			kpt[258].m_Present = 1; kpt[258].m_ReadWriter = 0;
			kpt[259].m_PageBaseAddress = newPage >> 12;
			kpt[259].m_Present = 1; kpt[259].m_ReadWriter = 1;
			FlushPageDirectory();
			const unsigned char* s = (const unsigned char*)(0xC0000000u + 258u * 0x1000u);
			unsigned char*       d = (unsigned char*)(0xC0000000u + 259u * 0x1000u);
			for (int i = 0; i < 0x1000; i++) d[i] = s[i];
			kpt[258].m_PageBaseAddress = f1;
			kpt[259].m_PageBaseAddress = f2;
			FlushPageDirectory();
			md.MapPageDirect(pageVA, newPage >> 12, true);
		} else {
			/* 唯一所有者：直接改为可写 */
			md.MapPageDirect(pageVA, spte.m_PageBaseAddress, true);
		}
		return;
	}

	/* 不应到达 */
	Diagnose::Write("PageFault unhandled cr2=%x err=%x\n", cr2, context->error_code);
	current->PSignal(User::SIGSEGV);
	if (current->IsSig())
		current->PSig((pt_context*)&context->eip);

	/* Old code removed — replaced by demand-paging handler above.
	 * Keeping the brace structure intact. */
	if (false) {
	/*��ȱҳ�쳣��������ÿ����չһҳ�����������ȱ�˶��Ŷ�ջҳ�棬�ǾͶ�ִ�м���ȱҳ�쳣��ֱ������Щҳ�油��*/

	if( (context->xcs & USER_MODE) == USER_MODE)
	{
		if( cr2 < MemoryDescriptor::USER_SPACE_SIZE - md.m_StackSize && cr2 >= context->esp - 8
				&& md.m_DataSize + md.m_StackSize + PageManager::PAGE_SIZE < MemoryDescriptor::USER_SPACE_SIZE - md.m_DataStartAddress )
			current->SStack();
		else
		{
			Diagnose::Write("Invalid MM access");
			current -> PSignal(User::SIGSEGV);
			if ( current->IsSig() )
				current->PSig( (pt_context *)&context->eip );
		}
	}
	else
		Utility::Panic("Page Fault in Kernel Mode.");
	} /* end if(false) */
}

//x87 FPU�������(INT 16)
IMPLEMENT_EXCEPTION_ENTRANCE(CoprocessorErrorEntrance, CoprocessorError)
IMPLEMENT_EXCEPTION_HANDLER(CoprocessorError, "Coprocessor Error!", User::SIGFPE)


//����У��(INT 17)  *�г�����*
IMPLEMENT_EXCEPTION_ENTRANCE_ERRCODE(AlignmentCheckEntrance, AlignmentCheck)
IMPLEMENT_EXCEPTION_HANDLER_ERRCODE(AlignmentCheck, "Alignment Check!", User::SIGBUS)


//�������(INT 18)
IMPLEMENT_EXCEPTION_ENTRANCE(MachineCheckEntrance, MachineCheck)
IMPLEMENT_EXCEPTION_HANDLER(MachineCheck, "Machine Check!", User::SIGNUL)


//SIMD�����쳣(INT 19)
IMPLEMENT_EXCEPTION_ENTRANCE(SIMDExceptionEntrance, SIMDException)
IMPLEMENT_EXCEPTION_HANDLER(SIMDException, "SIMD Float Point Exception!", User::SIGFPE)
