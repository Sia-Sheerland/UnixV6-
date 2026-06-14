#include "ProcessManager.h"
#include "Machine.h"
#include "User.h"
#include "Kernel.h"
#include "Video.h"
#include "Utility.h"
#include "PEParser.h"
#include "Regs.h"
#include "MemoryDescriptor.h"
#include "VmAreaStruct.h"
#include "PageRefCount.h"
#include "OpenFileManager.h"

unsigned int ProcessManager::m_NextUniquePid = 0;

ProcessManager::ProcessManager()
{
	CurPri = 0;
	RunRun = 0;
	RunIn = 0;
	RunOut = 0;
	ExeCnt = 0;
	SwtchNum = 0;
}

ProcessManager::~ProcessManager()
{
}

void ProcessManager::Initialize()
{
	//nothing to do here
}

void ProcessManager::SetupProcessZero()
{
	//��ʼ��Process#0��Process��User�ṹ
	Process* pProcZero = &(this->process[0]);
	pProcZero->p_stat = Process::SRUN;
	pProcZero->p_flag = Process::SLOAD | Process::SSYS;
	pProcZero->p_nice = 0;
	pProcZero->p_time = 0;
	pProcZero->p_pid = NextUniquePid();
	//��ppda�������ջ�⣬����û���û�̬����
	pProcZero->p_size = 0x1000;
	pProcZero->p_addr = PROCESS_ZERO_PPDA_ADDRESS;
	pProcZero->p_textp = NULL;

	User& u = Kernel::Instance().GetUser();
	u.u_procp = pProcZero;
	u.u_MemoryDescriptor.m_TextStartAddress = 0;
	u.u_MemoryDescriptor.m_TextSize = 0;
	u.u_MemoryDescriptor.m_DataStartAddress = 0;
	u.u_MemoryDescriptor.m_DataSize = 0;
	u.u_MemoryDescriptor.m_StackSize = 0;
	u.u_MemoryDescriptor.m_UserPageTableArray = NULL;

	/* Clear open file table to avoid stale entries after a warm reboot.
	 * Physical RAM at 0x3FF000 retains previous session state, so
	 * AllocFreeSlot() would skip fd=0 and return fd=1 → "STDIN Error!". */
	for (int i = 0; i < OpenFiles::NOFILES; i++)
		u.u_ofiles.SetF(i, NULL);
//	u.u_MemoryDescriptor.Initialize();
}

unsigned int ProcessManager::NextUniquePid()
{
	return ProcessManager::m_NextUniquePid++;
}

/* ================================================================
 * NewProc() — fork 系统调用的底层实现（写时复制版）
 *
 * 新语义：
 *   1. 只复制父进程的 ppda（1 页 = USIZE）；
 *   2. 克隆父进程的 VMA 链表给子进程；
 *   3. 将父/子进程的所有 R/W 用户页均标记为 R/O（COW 保护）；
 *   4. 增加所有共享物理页框的引用计数；
 *   5. 子进程 p_size = USIZE，p_addr = 新分配的 ppda 物理地址。
 * ================================================================ */
int ProcessManager::NewProc()
{
	Process* child = 0;
	for (int i = 0; i < ProcessManager::NPROC; i++) {
		if (process[i].p_stat == Process::SNULL) {
			child = &process[i];
			break;
		}
	}
	if (!child)
		Utility::Panic("No Proc Entry!");

	User&    u       = Kernel::Instance().GetUser();
	Process* current = (Process*)u.u_procp;

	/* 复制 Process 结构 */
	current->Clone(*child);
	child->p_size = ProcessManager::USIZE; /* 子进程只拥有 ppda */

	/* 子进程继承父进程共享代码段引用，必须增加引用计数，
	 * 否则子进程 exec() 调用 XFree() 时会将 x_count/x_ccount 减为 0，
	 * 导致父进程仍在使用的代码段物理页被释放。 */
	if (child->p_textp) {
		child->p_textp->x_count++;
		child->p_textp->x_ccount++;
	}

	SaveU(u.u_rsav);

	/* Child detection: when the child process is scheduled and RestoredU
	 * here, u.u_procp == child (child's ppda is active), while current
	 * still holds the parent's process pointer from the copied stack.
	 * Return 1 so Fork() knows we are the child. */
	if (u.u_procp == child) {
		return 1;
	}

	/* -------- 为子进程分配新的 shadow 页表 -------- */
	PageTable* parentPgTable = u.u_MemoryDescriptor.m_UserPageTableArray;
	u.u_MemoryDescriptor.Initialize(); /* 在 u（当前=父进程上下文）中临时分配，用于子进程 */
	PageTable* childPgTable = u.u_MemoryDescriptor.m_UserPageTableArray;

	/* 将父进程的 shadow 页表内容复制给子进程，
	 * 同时将所有 R/W 页标记为 R/O（COW 保护），并统计引用计数。 */
	if (parentPgTable) {
		for (unsigned int ti = 0; ti < Machine::USER_PAGE_TABLE_CNT; ti++) {
			for (unsigned int ei = 0; ei < PageTable::ENTRY_CNT_PER_PAGETABLE; ei++) {
				PageTableEntry& src  = parentPgTable[ti].m_Entrys[ei];
				PageTableEntry& cdst = childPgTable[ti].m_Entrys[ei];

				cdst = src; /* 子进程页表项 = 父进程页表项 */

				if (src.m_Present) {
					unsigned long physAddr =
						(unsigned long)src.m_PageBaseAddress << 12;

					/* 若为 R/W 页：父进程和子进程均改为 R/O（等待 COW） */
					if (src.m_ReadWriter) {
						src.m_ReadWriter  = 0;
						cdst.m_ReadWriter = 0;
					}
					/* 增加共享页的引用计数 */
					PageRefCount::Inc(physAddr);
				}
			}
		}
		/* 将父进程修改后的 shadow 同步到硬件 */
		u.u_MemoryDescriptor.m_UserPageTableArray = parentPgTable;
		u.u_MemoryDescriptor.MapToPageTable();
	}

	/* -------- 克隆 VMA 链表 -------- */
	VmAreaStruct* childVmaList = u.u_MemoryDescriptor.CloneVmaList();

	/* -------- 切换 u_procp 到子进程，写入子进程的 ppda -------- */
	u.u_procp = child;

	UserPageManager& upm = Kernel::Instance().GetUserPageManager();

	/* 为子进程分配 ppda（仅 USIZE = 1 页） */
	unsigned long childPpda = upm.AllocMemory(ProcessManager::USIZE);
	if (!childPpda) {
		/* 内存不足：使用 swap（仅换 ppda） */
		current->p_stat = Process::SIDL;
		child->p_addr   = current->p_addr;
		SaveU(u.u_ssav);
		this->XSwap(child, false, ProcessManager::USIZE);
		child->p_flag |= Process::SSWAP;
		current->p_stat = Process::SRUN;
	} else {
		child->p_addr = childPpda;
		/* 复制父进程 ppda 内容到子进程 */
		unsigned long srcPpda = current->p_addr;
		for (unsigned int b = 0; b < ProcessManager::USIZE; b++) {
			Utility::CopySeg(srcPpda + b, childPpda + b);
		}
	}

	/* -------- 在子进程的 ppda 中设置 VMA 链表和 shadow 页表 -------- */
	/* 借用 SwtchUStruct 临时访问子进程的 User 结构（不调用 RetU，避免跳转） */
	SwtchUStruct(child);
	{
		User& cu = Kernel::Instance().GetUser();
		cu.u_procp                                 = child;
		cu.u_MemoryDescriptor.m_UserPageTableArray = childPgTable;
		cu.u_MemoryDescriptor.m_VmaList            = childVmaList;
		cu.u_MemoryDescriptor.m_TextStartAddress   = u.u_MemoryDescriptor.m_TextStartAddress;
		cu.u_MemoryDescriptor.m_TextSize           = u.u_MemoryDescriptor.m_TextSize;
		cu.u_MemoryDescriptor.m_DataStartAddress   = u.u_MemoryDescriptor.m_DataStartAddress;
		cu.u_MemoryDescriptor.m_DataSize           = u.u_MemoryDescriptor.m_DataSize;
		cu.u_MemoryDescriptor.m_StackSize          = u.u_MemoryDescriptor.m_StackSize;
		cu.u_MemoryDescriptor.m_HeapStart          = u.u_MemoryDescriptor.m_HeapStart;
		cu.u_MemoryDescriptor.m_HeapEnd            = u.u_MemoryDescriptor.m_HeapEnd;
	}
	/* 切换回父进程 ppda */
	SwtchUStruct(current);

	u.u_procp = current;
	u.u_MemoryDescriptor.m_UserPageTableArray = parentPgTable;

	/* Remove old unreachable code that followed */
	if (false) {
	/* Newproc�������ֳ������֣�clone������process�ṹ�ڵ�����
	current->Clone(*child);

	/* ���������Ҫ����SaveU()�����ֳ���u������Ϊ��Щ���̲���һ��
	���ù� */
	SaveU(u.u_rsav);

	/* �������̵��û�̬ҳ��ָ��m_UserPageTableArray������pgTable */
	PageTable* pgTable = u.u_MemoryDescriptor.m_UserPageTableArray;
	u.u_MemoryDescriptor.Initialize();
	/* �����̵���Ե�ַӳ�ձ��������ӽ��̣�������ҳ���Ĵ�С */
	if ( NULL != pgTable )
	{
		// u.u_MemoryDescriptor.Initialize();
		Utility::MemCopy((unsigned long)pgTable, (unsigned long)u.u_MemoryDescriptor.m_UserPageTableArray, sizeof(PageTable) * MemoryDescriptor::USER_SPACE_PAGE_TABLE_CNT);
	}

	//�������н��̵�u����u_procpָ��new process
	//���������ڱ����Ƶ�ʱ�����ֱ�Ӹ���u_procp��
	//��ַ�����ڴ治��ʱ�����޷���u��ӳ�䵽�û�����
	//�޸�u_procp�ĵ�ַ��
	u.u_procp = child;

	UserPageManager& userPageManager = Kernel::Instance().GetUserPageManager();

	unsigned long srcAddress = current->p_addr;
	unsigned long desAddress = userPageManager.AllocMemory(current->p_size);
	//Diagnose::Write("srcAddress %x\n", srcAddress);
	//Diagnose::Write("desAddress %x\n", desAddress);
	if ( desAddress == 0 ) /* �ڴ治������Ҫswap */
	{
		current->p_stat = Process::SIDL;
		/* �ӽ���p_addrָ�򸸽���ͼ����Ϊ�ӽ��̻�������������Ҫ�Ը�����ͼ��Ϊ���� */
		child->p_addr = current->p_addr;
		SaveU(u.u_ssav);
		this->XSwap(child, false, 0);
		child->p_flag |= Process::SSWAP;
		current->p_stat = Process::SRUN;
	}
	else
	{
		int n = current->p_size;
		child->p_addr = desAddress;
		while (n--)
		{
			Utility::CopySeg(srcAddress++, desAddress++);
		}
	}
	u.u_procp = current;
	/* 
	 * ��������ͼ���ڼ䣬�����̵�m_UserPageTableArrayָ���ӽ��̵���Ե�ַӳ�ձ���
	 * ������ɺ���ָܻ�Ϊ��ǰ���ݵ�pgTable��
	 */
	u.u_MemoryDescriptor.m_UserPageTableArray = pgTable;
	} /* end if(false) — old code */

	return 0;
}

/*�ڽ����л��Ĺ����У�����û���õ�TSS */
int ProcessManager::Swtch()
{	
	//Diagnose::Write("Start Swtch()\n");
	User& u = Kernel::Instance().GetUser();
	SaveU(u.u_rsav);

	/* 0#������̨*/
	Process* procZero = &process[0];

	/* 
	 * ��SwtchUStruct()��RetU()��Ϊ�ٽ�������ֹ���жϴ�ϡ�
	 * �����RetU()�ָ�esp֮����δ�ָ�ebpʱ���жϽ���ᵼ��
	 * esp��ebp�ֱ�ָ��������ͬ���̵ĺ���ջ��λ�á� good comment��
	 *
	 * Ϊʲô����0#���̳е���ѡ����������̨�Ĳ�����
	 * ���ӽ����л��ĽǶȣ���ȫ��������̨������ѡ����������̨�� ���ǣ�����ʱ���жϡ�
	 * һ��ĩ�� ���д��������ϵͳidleʱ���������ִ��Ӧ�ó�������У������Է����ں�ִ�й����С�
	 * ����жϣ�
	 * �ں�idle�ı�־��  0#������˯��ִ̬��idle()�ӳ���
	 * �� TimeInterrupt.cpp��Line 82.
	 * ���ǣ�������0#����ִ��select()��
	 *
	 */
	X86Assembly::CLI();
	SwtchUStruct(procZero);
	RetU();
	X86Assembly::STI();

	/* ��ѡ���ʺ���̨�Ľ��� */
	Process* selected = Select();

	/* �ָ���������̵��ֳ� */
	X86Assembly::CLI();
	SwtchUStruct(selected);
	RetU();
	X86Assembly::STI();

	User& newu = Kernel::Instance().GetUser();
	newu.u_MemoryDescriptor.MapToPageTable();
	
	/*
	 * If the new process paused because it was
	 * swapped out, set the stack level to the last call
	 * to savu(u_ssav).  This means that the return
	 * which is executed immediately after the call to aretu
	 * actually returns from the last routine which did
	 * the savu.
	 *
	 * You are not expected to understand this.
	 */
	if ( newu.u_procp->p_flag & Process::SSWAP )
	{
		newu.u_procp->p_flag &= ~Process::SSWAP;
		aRetU(newu.u_ssav);
	}
	
	/* 
	 * ��fork���Ľ�������̨֮ǰ���ڱ�������̨ʱ����1��
	 * ��ͬʱ���ص�NewProc()ִ�еĵ�ַ
	 */
	return 1;
}

void ProcessManager::Sched()
{
	Process* pSelected;
	User& u = Kernel::Instance().GetUser();
	int seconds;
	unsigned int size;
	unsigned long desAddress;

	/* 
	 * ѡ���ڽ�����פ��ʱ��������ھ���״̬�Ľ��̻���
	 */
	goto loop;

sloop:
	this->RunIn++;
	u.u_procp->Sleep((unsigned long)&RunIn, ProcessManager::PSWP);

loop:
	X86Assembly::CLI();
	seconds = -1;
	for ( int i = 0; i < ProcessManager::NPROC; i++ )
	{
		if ( this->process[i].p_stat == Process::SRUN && (this->process[i].p_flag & Process::SLOAD) == 0 && this->process[i].p_time > seconds )
		{
			pSelected = &(this->process[i]);
			seconds = pSelected->p_time;
		}
	}

	/* ���û�з��������Ľ��̣�0#����˯�ߵȴ�����Ҫ����Ľ��� */
	if ( -1 == seconds )
	{
		this->RunOut++;
		u.u_procp->Sleep((unsigned long)&RunOut, ProcessManager::PSWP);
		goto loop;
	}

	/* ����н���������������Ҫ���룬�����Ƿ����㹻�ڴ� */
	X86Assembly::STI();
	/* ������̻�����Ҫ���ڴ��С */
	size = pSelected->p_size;
	/* 
	 * ������ڹ������ĶΣ�����û�н���ͼ�����ڴ��У����ø����ĶεĽ��̣�
	 * ���������Ķβ����ڴ��У�����ʱ��Ҫ�������Ķ��ڽ������еĸ���
	 */
	if ( pSelected->p_textp != NULL && 0 == pSelected->p_textp->x_ccount )
	{
		size += pSelected->p_textp->x_size;
	}
	/* ����ڴ����ɹ��������ʵ�ʻ������ */
	desAddress = Kernel::Instance().GetUserPageManager().AllocMemory(size);
	if ( NULL != desAddress )
	{
		goto found2;
	}

	/*
	 * �����ڴ�ʧ������£������ڴ��н��̣��ڳ��ռ䡣
	 * ����ԭ�򣺴��׵��ѣ����ν�������Ȩ˯��״̬(SWAIT)-->
	 * ��ͣ״̬(SSTOP)-->������Ȩ˯��״̬(SSLEEP)-->����״̬(SRUN)���̻�����
	 */
	X86Assembly::CLI();
	for ( int i = 0; i < ProcessManager::NPROC; i++ )
	{
		if ( this->process[i].p_flag & (Process::SSYS | Process::SLOCK | Process::SLOAD) == Process::SLOAD && (this->process[i].p_stat == Process::SWAIT || this->process[i].p_stat == Process::SSTOP) )
		{
			goto found1;
		}
	}

	/* 
	 * �ڻ���������Ȩ˯��״̬(SSLEEP)������״̬(SRUN)���̶��ڳ��ڴ�֮ǰ��
	 * ������������ڽ�����פ��ʱ���Ƿ��Ѵﵽ3�룬�������軻��
	 */
	if ( seconds < 3 )
	{
		goto sloop;
	}

	seconds = -1;
	for ( int i = 0; i < ProcessManager::NPROC; i++ )
	{
		if ( this->process[i].p_flag & (Process::SSYS | Process::SLOCK | Process::SLOAD) == Process::SLOAD && (this->process[i].p_stat == Process::SWAIT || this->process[i].p_stat == Process::SSTOP) && pSelected->p_time > seconds )
		{
			pSelected = &(this->process[i]);
			seconds = pSelected->p_time;
		}
	}

	/* ���Ҫ����SSLEEP��SRUN״̬���̣��ȼ��ý���פ���ڴ�ʱ���Ƿ񳬹�2�룬�����軻�� */
	if ( seconds < 2 )
	{
		goto sloop;
	}

	/* ����pSelectedָ��ı�ѡ�н��� */
found1:
	X86Assembly::STI();
	pSelected->p_flag &= ~Process::SLOAD;
	this->XSwap(pSelected, true, 0);
	/* �ڳ��ڴ�ռ���ٴγ��Ի������ */
	goto loop;

	/* �Ѿ�������㹻���ڴ棬����ʵ�ʵĻ������ */
found2:
	BufferManager& bufMgr = Kernel::Instance().GetBufferManager();
	/* 
	* ������ڹ������ĶΣ�����û�н���ͼ�����ڴ��У����ø����ĶεĽ��̣�
	* ���������Ķβ����ڴ��У�����ʱ��Ҫ�������Ķ��ڽ������еĸ���
	*/
	if ( pSelected->p_textp != NULL )
	{
		Text* pText = pSelected->p_textp;
		if ( pText->x_ccount == 0 )
		{
			/* ��Ϊ�������ĶΣ��ͽ���ppda�����ݶΡ���ջ���ڽ��������Ƿֿ���ŵģ������Ȼ��빲�����Ķ� */
			if ( bufMgr.Swap(pText->x_daddr, desAddress, pText->x_size, Buf::B_READ) == false )
			{
				goto err;
			}
			/* �������Ķ����ڴ��е���ʼ��ַ */
			pText->x_caddr = desAddress;
			desAddress += pText->x_size;
		}
		pText->x_ccount++;
	}
	/* ����ʣ�ಿ��ͼ��ppda�����ݶΡ���ջ�� */
	if ( bufMgr.Swap(pSelected->p_addr /* blkno */, desAddress, pSelected->p_size, Buf::B_READ) == false )
	{
		goto err;
	}
	Kernel::Instance().GetSwapperManager().FreeSwap(pSelected->p_size, pSelected->p_addr /* blkno */);
	pSelected->p_addr = desAddress;
	pSelected->p_flag |= Process::SLOAD;
	pSelected->p_time = 0;
	goto loop;

err:
	Utility::Panic("Swap Error");
}

void ProcessManager::Wait()
{
	int i;
	bool hasChild = false;
	User& u = Kernel::Instance().GetUser();
	SwapperManager& swapperMgr = Kernel::Instance().GetSwapperManager();
	BufferManager& bufMgr = Kernel::Instance().GetBufferManager();
	
	Diagnose::Write("Process %d finding dead son. They are ",u.u_procp->p_pid);
	while(true)
	{
		for ( i = 0; i < NPROC; i++ )
		{
			if ( u.u_procp->p_pid == process[i].p_ppid )
			{
				Diagnose::Write("Process %d (Status:%d)  ",process[i].p_pid,process[i].p_stat);
				hasChild = true;
				/* ˯�ߵȴ�ֱ���ӽ��̽��� */
				if( Process::SZOMB == process[i].p_stat )
				{
					/* wait()ϵͳ���÷����ӽ��̵�pid */
					u.u_ar0[User::EAX] = process[i].p_pid;

					process[i].p_stat = Process::SNULL;
					process[i].p_pid = 0;
					process[i].p_ppid = -1;
					process[i].p_sig = 0;
					process[i].p_flag = 0;

					/* ����swapper���ӽ���u�ṹ���� */
					Buf* pBuf = bufMgr.Bread(DeviceManager::ROOTDEV, process[i].p_addr);
					swapperMgr.FreeSwap(BufferManager::BUFFER_SIZE, process[i].p_addr);
					User* pUser = (User *)pBuf->b_addr;

					/* ���ӽ��̵�ʱ��ӵ��������� */
					u.u_cstime += pUser->u_cstime +	pUser->u_stime;
					u.u_cutime += pUser->u_cutime + pUser->u_utime;

					int* pInt = (int *)u.u_arg[0];
					/* ��ȡ�ӽ���exit(int status)�ķ���ֵ */
					*pInt = pUser->u_arg[0];

					/* ����˴�û��Brelse()ϵͳ�ᷢ��ʲô-_- */
					bufMgr.Brelse(pBuf);
					Diagnose::Write("end wait\n");
					return;
				}
			}
		}
		if (true == hasChild)
		{
			/* ˯�ߵȴ�ֱ���ӽ��̽��� */
			Diagnose::Write("wait until child process Exit! ");
			u.u_procp->Sleep((unsigned long)u.u_procp, ProcessManager::PWAIT);
			Diagnose::Write("end sleep\n");
			continue;	/* �ص����while(true)ѭ�� */
		}
		else
		{
			/* ��������Ҫ�ȴ��������ӽ��̣����ó����룬wait()���� */
			u.u_error = User::ECHILD;
			break;	/* Get out of while loop */
		}
	}
}

void ProcessManager::Fork()
{
	User& u = Kernel::Instance().GetUser();
	Process* child = NULL;;

	/* Ѱ�ҿ��е�process���Ϊ�ӽ��̵Ľ��̿��ƿ� */
	for ( int i = 0; i < ProcessManager::NPROC; i++ )
	{
		if ( this->process[i].p_stat == Process::SNULL )
		{
			child = &this->process[i];
			break;
		}
	}
	if ( child == NULL )
	{
		/* û�п���process������� */
		u.u_error = User::EAGAIN;
		return;
	}

	if ( this->NewProc() )	/* �ӽ��̷���1�������̷���0 */
	{
		/* �ӽ���fork()ϵͳ���÷���0 */
		u.u_ar0[User::EAX] = 0;
		u.u_cstime = 0;
		u.u_stime = 0;
		u.u_cutime = 0;
		u.u_utime = 0;
	}
	else
	{
		/* �����̽���fork()ϵͳ���÷����ӽ���PID */
		u.u_ar0[User::EAX] = child->p_pid;
	}

	return;
}

extern "C" void runtime();
extern "C" void ExecShell();

/* ================================================================
 * 释放当前进程在页表中已单独分配的所有用户物理页
 * （exec / exit 时调用，用于新的按需分配模型）
 * ================================================================ */
void ProcessManager::FreeUserPages(User& u)
{
    UserPageManager& upm = Kernel::Instance().GetUserPageManager();
    MemoryDescriptor& md = u.u_MemoryDescriptor;
    if (!md.m_UserPageTableArray) return;

    /* 跳过代码段物理帧（由 Text 结构统一管理） */
    unsigned long textFrmBase = 0, textFrmEnd = 0;
    if (u.u_procp->p_textp) {
        textFrmBase = u.u_procp->p_textp->x_caddr >> 12;
        textFrmEnd  = textFrmBase
                      + ((u.u_procp->p_textp->x_size + PageManager::PAGE_SIZE - 1)
                         / PageManager::PAGE_SIZE);
    }

    for (unsigned int ti = 0; ti < Machine::USER_PAGE_TABLE_CNT; ti++) {
        for (unsigned int ei = 0; ei < PageTable::ENTRY_CNT_PER_PAGETABLE; ei++) {
            PageTableEntry& pte = md.m_UserPageTableArray[ti].m_Entrys[ei];
            if (!pte.m_Present && pte.m_PageBaseAddress == 0) continue;

            unsigned long frm = pte.m_PageBaseAddress;

            /* 跳过文本段页框 */
            if (frm >= textFrmBase && frm < textFrmEnd) {
                pte.m_Present = 0;
                pte.m_PageBaseAddress = 0;
                continue;
            }

            unsigned long physAddr = frm << 12;

            if (pte.m_Present) {
                /* 正常映射页：减引用，引用为零时释放 */
                PageRefCount::Dec(physAddr);
                if (PageRefCount::Get(physAddr) == 0)
                    upm.FreeMemory(PageManager::PAGE_SIZE, physAddr);
            } else {
                /* P=0 但有帧号：已换出至 Swap，释放 Swap 块 */
                Kernel::Instance().GetSwapperManager().FreeSwap(
                    PageManager::PAGE_SIZE, (int)frm);
            }

            pte.m_Present       = 0;
            pte.m_PageBaseAddress = 0;
        }
    }
    md.ClearUserPageTable();
}

/* ================================================================
 * Exec — 加载并执行新程序（按需调页版）
 *
 * 主要变化：
 *   1. 用 FreeUserPages() 逐页释放旧进程的数据/栈物理页；
 *   2. 释放旧 VMA 链表；
 *   3. 按节单独分配数据/BSS 物理页（可追踪引用，支持 COW）；
 *   4. 建立 VMA 链表描述各虚拟区域（代码/数据/BSS/堆/栈）；
 *   5. p_size 设置为 USIZE（仅 ppda），数据/栈页独立跟踪。
 * ================================================================ */
void ProcessManager::Exec()
{
	Inode* pInode;
	Text* pText;
	User& u = Kernel::Instance().GetUser();
	FileManager& fileMgr = Kernel::Instance().GetFileManager();
	UserPageManager& userPgMgr = Kernel::Instance().GetUserPageManager();
	KernelPageManager& kernelPgMgr = Kernel::Instance().GetKernelPageManager();
	BufferManager& bufMgr = Kernel::Instance().GetBufferManager();

	// Diagnose::Write("Process %d execing\n",u.u_procp->p_pid);
	pInode = fileMgr.NameI(FileManager::NextChar, FileManager::OPEN);
	if ( NULL == pInode )	//����Ŀ¼ʧ��
	{
		return;
	}

	/* ���ͬʱ����ͼ��Ļ��Ľ������������ƣ����Ƚ���˯�� */
	while( this->ExeCnt >= NEXEC )
	{
		u.u_procp->Sleep((unsigned long)&ExeCnt, ProcessManager::EXPRI);
	}
	this->ExeCnt++;

	/* ���̱���ӵ�п�ִ���ļ���ִ��Ȩ�ޣ��ұ�ִ�е�ֻ����һ���ļ��� */
	if ( fileMgr.Access(pInode, Inode::IEXEC) || (pInode->i_mode & Inode::IFMT) != 0 )
	{
		fileMgr.m_InodeTable->IPut(pInode);
		if ( this->ExeCnt >= NEXEC )
		{
			WakeUpAll((unsigned long)&ExeCnt);
		}
		this->ExeCnt--;
		return;
	}

	PEParser parser;

    if ( parser.HeaderLoad(pInode)==false )
    {
        fileMgr.m_InodeTable->IPut(pInode);
        return;
    }

 	/* ��ȡ����PEͷ�ṹ�õ����Ķε���ʼ��ַ������ */
	u.u_MemoryDescriptor.m_TextStartAddress = parser.TextAddress;
	u.u_MemoryDescriptor.m_TextSize = parser.TextSize;

	/* ���ݶε���ʼ��ַ������ */
	u.u_MemoryDescriptor.m_DataStartAddress = parser.DataAddress;
	u.u_MemoryDescriptor.m_DataSize = parser.DataSize;

	/* ��ջ�γ�ʼ������ */
	u.u_MemoryDescriptor.m_StackSize = parser.StackSize;
	
	if ( parser.TextSize + parser.DataSize + parser.StackSize  + PageManager::PAGE_SIZE > MemoryDescriptor::USER_SPACE_SIZE - parser.TextAddress)
	{
		fileMgr.m_InodeTable->IPut(pInode);
		u.u_error = User::ENOMEM;
		return;
	}

	/* 
	 * �����ڴ����ڴ���û�����������Ҫ�Ĳ���argc��argv[]����Щ������exec()ϵͳ���ô��룬
	 * λ�ڽ���ͼ��Ļ�ǰ���û�ջ�У����������ݵ�fakeStack�У�Ȼ������ͷ�ԭ����ͼ��
	 * ������½���ͼ��֮���ٽ�fakeStack�еı��ݲ����������½��̵��û�ջ�С�
	 */
	//unsigned long fakeStack = kernelPgMgr.AllocMemory(parser.StackSize);
	int allocLength = (parser.StackSize + PageManager::PAGE_SIZE * 2 - 1) >> 13 << 13;
	unsigned long fakeStack = kernelPgMgr.AllocMemory(allocLength);

	int argc = u.u_arg[1];
	char** argv = (char **)u.u_arg[2];

	/* esp��λ��ջ�� */
	unsigned int esp = MemoryDescriptor::USER_SPACE_SIZE;
	/* ʹ�ú���̬ҳ��ӳ�䣬������������ַ�ϼ�0xC0000000�������Ե�ַ */
	unsigned long desAddress = fakeStack + allocLength + 0xC0000000;
	//unsigned long desAddress = fakeStack + parser.StackSize + 0xC0000000;
	int length;

	/* ����argv[]ָ������ָ��������в����ַ��� */
	for (int i = 0; i < argc; i++ )
	{
		length = 0;
		/* ��������ַ������ȣ�length����'\0' */
		while( NULL != argv[i][length] )
		{
			length++;
		}
		desAddress = desAddress - (length + 1);
		/* ����ʱ��'\0'һ�𿽱���ȥ */
		Utility::MemCopy((unsigned long)argv[i], desAddress, length + 1);
		/* �������ַ������½���ͼ���û�ջ�е���ʼλ�ô���argv[i]���û�ջλ�ڽ����߼���ַ�ռ�0x800000�ĵײ� */
		esp = esp - (length + 1);
		argv[i] = (char *)esp;
	}

	/* ������ŵ���int����ֵ��������16�ֽڱ߽���� */
	desAddress = desAddress & 0xFFFFFFF0;
	esp = esp & 0xFFFFFFF0;

	/* ����argc��argv[] */
	int endValue = 0;
	desAddress -= sizeof(endValue);
	esp -= sizeof(endValue);
	/* ���û�ջ��д��endValue��Ϊargv[]�Ľ��� */
	Utility::MemCopy((unsigned long)&endValue, desAddress, sizeof(endValue));

	desAddress -= argc * sizeof(int);
	esp -= argc * sizeof(int);
	/* д��argv[]������ */
	Utility::MemCopy((unsigned long)argv, desAddress, argc * sizeof(int));

	/* ��endValueָ��ǰջ��argv[]����ʼ��ַ����argv[]��ջ��Ϻ�ǰջ����ַ */
	endValue = esp;
	desAddress -= sizeof(int);
	esp -= sizeof(int);
	Utility::MemCopy((unsigned long)&endValue, desAddress, sizeof(int));

	/* �����ջargc */
	desAddress -= sizeof(int);
	esp -= sizeof(int);
	Utility::MemCopy((unsigned long)&argc, desAddress, sizeof(int));	/* Done! */


    /* 释放旧进程的用户物理页和 VMA 链表 */
    FreeUserPages(u);
    u.u_MemoryDescriptor.FreeAllVmas();

    /* 释放旧共享代码段引用 */
    if ( u.u_procp->p_textp != NULL )
    {
        u.u_procp->p_textp->XFree();
        u.u_procp->p_textp = NULL;
    }
    u.u_procp->p_size = ProcessManager::USIZE;

    pText = NULL;
    /* 查找空闲 Text 结构，或与已执行该程序的进程共享代码段 */
    for ( int i = 0; i < ProcessManager::NTEXT; i++ )
    {
        if ( NULL == this->text[i].x_iptr )
        {
            if ( NULL == pText )
                pText = &(this->text[i]);
        }
        else if ( pInode == this->text[i].x_iptr )
        {
            this->text[i].x_count++;
            this->text[i].x_ccount++;
            u.u_procp->p_textp = &(this->text[i]);
            pText = NULL;
            break;
        }
    }

    int sharedText = 0;

    if ( NULL != pText )
    {
        pInode->i_count++;
        pText->x_ccount = 1;
        pText->x_count  = 1;
        pText->x_iptr   = pInode;
        pText->x_size   = u.u_MemoryDescriptor.m_TextSize;
        pText->x_caddr  = userPgMgr.AllocMemory(pText->x_size);
        pText->x_daddr  = Kernel::Instance().GetSwapperManager().AllocSwap(pText->x_size);
        u.u_procp->p_textp = pText;
    }
    else
    {
        pText     = u.u_procp->p_textp;
        sharedText = 1;
    }

    /* 清空 shadow 页表，然后逐页建立映射 */
    u.u_MemoryDescriptor.ClearUserPageTable();

    /* 映射代码段页（暂为 R/W，Relocate 写入后改为 R/O） */
    if ( sharedText == 0 && pText && parser.TextSize > 0 )
    {
        unsigned long textPhysBase = pText->x_caddr;
        unsigned long textPages = (parser.TextSize + PageManager::PAGE_SIZE - 1) / PageManager::PAGE_SIZE;
        for (unsigned long p = 0; p < textPages; p++)
        {
            unsigned long va  = parser.TextAddress + p * PageManager::PAGE_SIZE;
            unsigned long frm = (textPhysBase + p * PageManager::PAGE_SIZE) >> 12;
            u.u_MemoryDescriptor.MapPageDirect(va, frm, true);
        }
    }

    /* 逐页分配数据段物理页，引用计数 = 1 */
    {
        unsigned long dataPages = (parser.DataSize + PageManager::PAGE_SIZE - 1) / PageManager::PAGE_SIZE;
        for (unsigned long p = 0; p < dataPages; p++)
        {
            unsigned long va       = parser.DataAddress + p * PageManager::PAGE_SIZE;
            unsigned long physPage = userPgMgr.AllocMemory(PageManager::PAGE_SIZE);
            if (!physPage) { u.u_error = User::ENOMEM; goto exec_cleanup; }
            PageRefCount::Set(physPage, 1);
            u.u_MemoryDescriptor.MapPageDirect(va, physPage >> 12, true);
        }
    }

    /* 分配并映射栈页（用于 fakeStack 传参，其余部分按需分配） */
    {
        unsigned long stackBase  = MemoryDescriptor::USER_SPACE_SIZE - parser.StackSize;
        unsigned long stackPages = (parser.StackSize + PageManager::PAGE_SIZE - 1) / PageManager::PAGE_SIZE;
        for (unsigned long p = 0; p < stackPages; p++)
        {
            unsigned long va       = stackBase + p * PageManager::PAGE_SIZE;
            unsigned long physPage = userPgMgr.AllocMemory(PageManager::PAGE_SIZE);
            if (!physPage) { u.u_error = User::ENOMEM; goto exec_cleanup; }
            PageRefCount::Set(physPage, 1);
            u.u_MemoryDescriptor.MapPageDirect(va, physPage >> 12, true);
        }
    }

    /* 同步到硬件页表 */
    u.u_MemoryDescriptor.MapToPageTable();

    Diagnose::Write("Process %x, p_addr %x, x_addr %x, p_size %x, x_size %x\n",
            u.u_procp->p_pid, u.u_procp->p_addr,
            u.u_procp->p_textp->x_caddr, u.u_procp->p_size,
            u.u_procp->p_textp->x_size);

    /* 从 exe 文件加载 .text/.data/.rdata 各节 */
    parser.Relocate(pInode, sharedText);

    /* 将 .text 写入 swap，并将代码段页改为只读 */
    if ( sharedText == 0 )
    {
        u.u_procp->p_flag |= Process::SLOCK;
        bufMgr.Swap(pText->x_daddr, pText->x_caddr, pText->x_size, Buf::B_WRITE);
        u.u_procp->p_flag &= ~Process::SLOCK;

        unsigned long textPages = (parser.TextSize + PageManager::PAGE_SIZE - 1) / PageManager::PAGE_SIZE;
        for (unsigned long p = 0; p < textPages; p++)
        {
            unsigned long va = parser.TextAddress + p * PageManager::PAGE_SIZE;
            u.u_MemoryDescriptor.GetPTE(va).m_ReadWriter = 0;
        }
        u.u_MemoryDescriptor.MapToPageTable();
    }

    /* 将 fakeStack 内容复制到新进程的用户栈 */
    Utility::MemCopy(fakeStack + allocLength - parser.StackSize | 0xC0000000,
                     MemoryDescriptor::USER_SPACE_SIZE - parser.StackSize,
                     parser.StackSize);
    kernelPgMgr.FreeMemory(allocLength, fakeStack);

    /* 建立 VMA 链表，描述各虚拟地址区域 */
    {
        unsigned long textEnd  = parser.TextAddress + parser.TextSize;
        unsigned long dataEnd  = parser.DataAddress + parser.DataSize;
        unsigned long bssStart = dataEnd;
        unsigned long bssEnd   = bssStart;
        if (parser.NumSections > PEParser::BSS_SECTION_IDX)
            bssEnd = bssStart + parser.SectionVirtSize[PEParser::BSS_SECTION_IDX];
        unsigned long heapStart = (bssEnd + PageManager::PAGE_SIZE - 1) & ~(PageManager::PAGE_SIZE - 1);
        unsigned long stackBase = MemoryDescriptor::USER_SPACE_SIZE - parser.StackSize;

        u.u_MemoryDescriptor.AddVma(parser.TextAddress, textEnd,
            VM_PROT_READ | VM_PROT_EXEC, VM_TEXT, pInode,
            parser.SectionFileOffset[PEParser::TEXT_SECTION_IDX], parser.TextSize);

        u.u_MemoryDescriptor.AddVma(parser.DataAddress, dataEnd,
            VM_PROT_READ | VM_PROT_WRITE, VM_DATA, pInode,
            parser.SectionFileOffset[PEParser::DATA_SECTION_IDX], parser.DataSize);

        if (bssEnd > bssStart)
            u.u_MemoryDescriptor.AddVma(bssStart, bssEnd,
                VM_PROT_READ | VM_PROT_WRITE, VM_BSS | VM_ANON, 0, 0, 0);

        u.u_MemoryDescriptor.AddVma(heapStart, heapStart,
            VM_PROT_READ | VM_PROT_WRITE, VM_HEAP | VM_ANON, 0, 0, 0);

        u.u_MemoryDescriptor.AddVma(stackBase, MemoryDescriptor::USER_SPACE_SIZE,
            VM_PROT_READ | VM_PROT_WRITE, VM_STACK | VM_ANON, 0, 0, 0);

        u.u_MemoryDescriptor.m_HeapStart = heapStart;
        u.u_MemoryDescriptor.m_HeapEnd   = heapStart;
    }

exec_cleanup:

	/* 
	  * ��runtime()��SignalHandler()���������������û�̬��ַ�ռ�0x00000000���Ե�ַ����runtime()
	  * ����ring0�˳���ring3��Ȩ��֮��ִ�еĴ��룬SignalHandler()Ϊ���̵��źŴ���������ڣ�����
	  * ���þ����źŵ�Handler��ÿһ������0x00000000���Ե�ַ����Ӧ����һ�ݶ�����runtime()��SignalHandler()
	  * ����������
	  */
//	unsigned char* runtimeSrc = (unsigned char*)runtime;
//	unsigned char* runtimeDst = 0x00000000;
//	for (unsigned int i = 0; i < (unsigned long)ExecShell - (unsigned long)runtime; i++)
//	{
//		*runtimeDst++ = *runtimeSrc++;
//	}

	/* �ͷ�Inode������ExeCnt����ֵ */
	fileMgr.m_InodeTable->IPut(pInode);
	if ( this->ExeCnt >= NEXEC )
	{
		WakeUpAll((unsigned long)&ExeCnt);
	}
	this->ExeCnt--;

	/* ��Ĭ�ϵķ�ʽ�����ź�  */
	for (int i = 0; i < u.NSIG ; i++)
	{
		u.u_signal[i] = 0;
	}

	/* ��0����ͨ�üĴ���  */
	for (int i = User::EAX - 4; i < User::EAX - 4*7 ; i = i - 4)
	{
		u.u_ar0[i] = 0;     /* �±�д��  User::EAX + i �ɶ���ҪǿһЩ�����������ٶ����ˡ���С�٣�׷���ٶȰ� */
	}

	/* ��exe�������ڵ�ַ�������ջ�ֳ��������е�EAX��Ϊϵͳ���÷���ֵ�������runtimeҪ��  */
	u.u_ar0[User::EAX] = parser.EntryPointAddress;
	
	/* �����Exec()ϵͳ���õ��˳�������ʹ֮�˳���ring3ʱ����ʼִ��user code */
	struct pt_context* pContext = (struct pt_context *)u.u_arg[4];
	pContext->eip = 0x00000000;	/* �˳���ring3��Ȩ���´����Ե�ַ0x00000000��runtime()��ʼִ�� */
	//pContext->eip = parser.EntryPointAddress;
	pContext->xcs = Machine::USER_CODE_SEGMENT_SELECTOR;
	pContext->eflags = 0x200;	/* �����Ƿ�۸��޹ؽ�Ҫ */
	pContext->esp = esp;
	pContext->xss = Machine::USER_DATA_SEGMENT_SELECTOR;
}

Process* ProcessManager::Select ()
{
	/* ǰһ��ѡ����̨���� */
	static int lastSelect = 0;
	
	while (true)
	{
		int priority = 256;
		int best = -1;	/* ���������ҵ����������̨���� */

		this->RunRun = 0;

		/* �������ȼ���ߵĿ����н��� */
		for ( int count = 0; count < NPROC ; count++ )
		{
			/* ����һ�α�ѡ�н��̵���һ����ʼ�ػ�ɨ�裬������ÿ�δ�0#���̿�ʼ����֤�����̻������ */
			int i = (lastSelect + 1 + count) % NPROC;
			if ( Process::SRUN == process[i].p_stat && (process[i].p_flag & Process::SLOAD) != 0 )
			{
				if ( process[i].p_pri < priority )
				{
					best = i;
					priority = process[i].p_pri;
				}
			}
		}
		if ( -1 == best )
		{
			__asm__ __volatile__("hlt");
			continue;
		}

		SwtchNum++;
		if ( SwtchNum & 0x80000000 ) 
		{
			SwtchNum = 0;	/* ���������Ϊ����������Ϊ�� */
		}
		/* ���ѡ�����ȼ���ߵĿ����н��� */
		this->CurPri = priority;
		lastSelect = best;
		//Diagnose::Write("Process %d is running!",best);
		return &process[best];

	}
}

void ProcessManager::Kill()
{
	User& u = Kernel::Instance().GetUser();
	int pid = u.u_arg[0];
	int signal = u.u_arg[1];
	bool flag = false;

	for ( int i = 0; i < ProcessManager::NPROC; i++ )
	{
		/* �����������źŸ��������� */
		if ( u.u_procp == &process[i] )
		{
			continue;
		}
		/* �����źŵĽ��շ�Ŀ����̣�������Ѱ */
		if ( pid != 0 && process[i].p_pid != pid)
		{
			continue;
		}
		/* pidΪ0�����źŷ������뷢�ͽ���ͬһ�ն˵����н��̣�0#���̲��������� */
		if ( pid == 0 && (process[i].p_ttyp != u.u_procp->p_ttyp || i == 0 ) )
		{
			continue;
		}
		/* �����ǳ����û�������Ҫ���͡����ս���u.uid��ͬ�������ɸ������û����̷����ź� */
		if ( u.u_uid != 0 && u.u_uid != process[i].p_uid )
		{
			continue;
		}
		flag = true;
		/* �źŷ��͸�����������Ŀ����� */
		process[i].PSignal(signal);
	}
	if ( false == flag )
	{
		u.u_error = User::ESRCH;
	}
}

void ProcessManager::WakeUpAll(unsigned long chan)
{
	/* ����ϵͳ��������chan������˯�ߵĽ��� */
	for(int i = 0; i < ProcessManager::NPROC; i++)
	{
		if( this->process[i].IsSleepOn(chan) )
		{
			this->process[i].SetRun();
		}
	}
}

void ProcessManager::XSwap( Process* pProcess, bool bFreeMemory, int size )
{
	if ( 0 == size)
	{
		size = pProcess->p_size;
	}

	/* blkno��¼���䵽�Ľ�������ʼ������ */
	int blkno = Kernel::Instance().GetSwapperManager().AllocSwap(pProcess->p_size);
	if ( 0 == blkno )
	{
		Utility::Panic("Out of Swapper Space");
	}
	/* �ݼ�����ͼ�����ڴ��У������ø����ĶεĽ����� */
	if ( pProcess->p_textp != NULL )
	{
		pProcess->p_textp->XccDec();
	}
	/* ��������ֹͬһ����ͼ���ظ����� */
	pProcess->p_flag |= Process::SLOCK;
	if ( false == Kernel::Instance().GetBufferManager().Swap(blkno, pProcess->p_addr, size, Buf::B_WRITE) )
	{
		Utility::Panic("Swap I/O Error");
	}
	if ( bFreeMemory )
	{
		Kernel::Instance().GetUserPageManager().FreeMemory(size, pProcess->p_addr);
	}
	/* �ѽ���ͼ���ڽ�������ʼ�����ż�¼��p_addr�У�SLOAD��0���������̽������ϵĽ����� */
	pProcess->p_addr = blkno;
	pProcess->p_flag &= ~(Process::SLOAD | Process::SLOCK);
	/* ���һ�α�����򻻳����������ڳ��򽻻���פ����ʱ�䳤������ */
	pProcess->p_time = 0;

	if ( this->RunOut )
	{
		this->RunOut = 0;
		Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)&RunOut);
	}
}

void ProcessManager::Signal( TTy* pTTy, int signal )
{
	for ( int i = 0; i < ProcessManager::NPROC; i++ )
	{
		if ( this->process[i].p_ttyp == pTTy )
		{
			this->process[i].PSignal(signal);
		}
	}
}
