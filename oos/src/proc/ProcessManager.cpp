#include "ProcessManager.h"
#include "Machine.h"
#include "User.h"
#include "Kernel.h"
#include "Video.h"
#include "Utility.h"
#include "PEParser.h"
#include "Regs.h"
#include "MemoryDescriptor.h"

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

// ------------- NOTE 3: ���̴���ҳĿ¼�������ں�ҳ�� -------------
PageDirectory *ProcessManager::AllocPageDirectory()
{
	// ! �ڴ�����Ա���ҳĿ¼�������Ե�ַ
	KernelPageManager &kernelPageManager = Kernel::Instance().GetKernelPageManager();
	return (PageDirectory *)(kernelPageManager.AllocMemory(sizeof(PageDirectory)) + Machine::KERNEL_SPACE_START_ADDRESS);
}

void ProcessManager::InitProcPageDirectory(Process *proc, PageTable *privateUsrPageTable)
{
	PageDirectory *pPageDirectory = proc->pPageDirectory;
	if (pPageDirectory)
	{
		// ���ù��õ�0#�û�ҳ��
		pPageDirectory->m_Entrys[0].m_UserSupervisor = 1;
		pPageDirectory->m_Entrys[0].m_Present = 1;
		pPageDirectory->m_Entrys[0].m_ReadWriter = 1;
		pPageDirectory->m_Entrys[0].m_PageTableBaseAddress = Machine::USER_PAGE_TABLE_BASE_ADDRESS >> 12;
		// ���ú���ҳ��
		unsigned int kPageTableIdx = Machine::KERNEL_SPACE_START_ADDRESS / PageTable::SIZE_PER_PAGETABLE_MAP;
		pPageDirectory->m_Entrys[kPageTableIdx].m_UserSupervisor = 0;
		pPageDirectory->m_Entrys[kPageTableIdx].m_Present = 1;
		pPageDirectory->m_Entrys[kPageTableIdx].m_ReadWriter = 1;
		pPageDirectory->m_Entrys[kPageTableIdx].m_PageTableBaseAddress = Machine::KERNEL_PAGE_TABLE_BASE_ADDRESS >> 12;
		// ����˽�е�1#�û�ҳ��
		unsigned long phyFrame = ((unsigned long)privateUsrPageTable - Machine::KERNEL_SPACE_START_ADDRESS) >> 12;
		pPageDirectory->m_Entrys[1].m_PageTableBaseAddress = phyFrame;
		pPageDirectory->m_Entrys[1].m_UserSupervisor = 1;
		pPageDirectory->m_Entrys[1].m_Present = 1;
		pPageDirectory->m_Entrys[1].m_ReadWriter = 1;
	}
}
// ------------- END NOTE 3 -------------

void ProcessManager::SetupProcessZero()
{
	// ��ʼ��Process#0��Process��User�ṹ
	Process *pProcZero = &(this->process[0]);
	pProcZero->p_stat = Process::SRUN;
	pProcZero->p_flag = Process::SLOAD | Process::SSYS;
	pProcZero->p_nice = 0;
	pProcZero->p_time = 0;
	pProcZero->p_pid = NextUniquePid();
	// ��ppda�������ջ�⣬����û���û�̬����
	pProcZero->p_size = 0x1000;
	pProcZero->p_addr = PROCESS_ZERO_PPDA_ADDRESS;
	pProcZero->p_textp = NULL;

	User &u = Kernel::Instance().GetUser();
	u.u_procp = pProcZero;
	u.u_MemoryDescriptor.m_TextStartAddress = 0;
	u.u_MemoryDescriptor.m_TextSize = 0;
	u.u_MemoryDescriptor.m_DataStartAddress = 0;
	u.u_MemoryDescriptor.m_DataSize = 0;
	u.u_MemoryDescriptor.m_StackSize = 0;
	u.u_MemoryDescriptor.m_UserPageTableArray = NULL;
	// u.u_MemoryDescriptor.Initialize();
	// ------------- NOTE 3: д0#����ҳĿ¼ -------------
	pProcZero->pPageDirectory = (PageDirectory *)(Machine::PAGE_DIRECTORY_BASE_ADDRESS + Machine::KERNEL_SPACE_START_ADDRESS);
	// ------------- END NOTE 3 -------------
}

unsigned int ProcessManager::NextUniquePid()
{
	return ProcessManager::m_NextUniquePid++;
}

// ------------- NOTE COW: -------------
void ProcessManager::ModifyPageTable(UserPageManager &userPageManager, PageTable *pgTable)
{
	for (unsigned int j = 0; j < PageTable::ENTRY_CNT_PER_PAGETABLE; j++)
	{
		if (pgTable->m_Entrys[j].m_Present && pgTable->m_Entrys[j].m_ReadWriter)
		{
			pgTable->m_Entrys[j].m_ReadWriter = 0;
			userPageManager.Page[pgTable->m_Entrys[j].m_PageBaseAddress]++;
		}
	}
}
// ------------- END NOTE COW -------------

int ProcessManager::NewProc()
{
	// NOTE 1: Find free process slot for child
	Process *child = 0;
	for (int i = 0; i < ProcessManager::NPROC; i++)
	{
		if (process[i].p_stat == Process::SNULL)
		{
			child = &process[i];
			break;
		}
	}
	if (!child)
	{
		Utility::Panic("No Proc Entry!");
	}

	// NOTE 2: Clone parent Process struct to child
	User &u = Kernel::Instance().GetUser();
	Process *current = (Process *)u.u_procp;
	current->Clone(*child);

	PageTable *pgTable = u.u_MemoryDescriptor.m_UserPageTableArray;  // save parent user page table

	// NOTE 3: Allocate private page table and page directory for child
	u.u_MemoryDescriptor.Initialize();  // allocate 1# user page table
	PageTable *desPgTable = u.u_MemoryDescriptor.m_UserPageTableArray;
	child->pPageDirectory = ProcessManager::AllocPageDirectory();   // allocate page directory
	ProcessManager::InitProcPageDirectory(child, desPgTable);   // write page directory

	// NOTE 4: Allocate PPDA page for child and copy parent PPDA
	UserPageManager &userPageManager = Kernel::Instance().GetUserPageManager();
	unsigned long desAddress = userPageManager.AllocMemory(PageManager::PAGE_SIZE);

	SaveU(u.u_rsav);
	u.u_procp = child;

	if (desAddress == 0)
	{
		/* no memory - swap not handled */
	}

	child->p_addr = desAddress;
	Utility::CopySeg(current->p_addr, desAddress);

	// NOTE 5: Copy parent 1# user page table to child (COW: mark pages read-only, increment ref count)
	if (pgTable)
	{
		// NOTE COW: mark parent pages read-only and increment reference counts
		ProcessManager::ModifyPageTable(userPageManager, pgTable);
		Utility::MemCopy((unsigned long)pgTable, (unsigned long)desPgTable, sizeof(PageTable));
	}

	u.u_procp = current;
	u.u_MemoryDescriptor.m_UserPageTableArray = pgTable;
	return 0;
}

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
			pText->x_caddr[0] = desAddress;
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

/* ���ڸҳ�Ϊ V6 �� execʵ�֡�ȱ������֧�� ISUID ���� */
void ProcessManager::Exec()
{
	Inode *pInode;
	Text *pText;
	User &u = Kernel::Instance().GetUser();
	FileManager &fileMgr = Kernel::Instance().GetFileManager();
	UserPageManager &userPgMgr = Kernel::Instance().GetUserPageManager();
	KernelPageManager &kernelPgMgr = Kernel::Instance().GetKernelPageManager();
	BufferManager &bufMgr = Kernel::Instance().GetBufferManager();

	Diagnose::Write("Process %d execing\n", u.u_procp->p_pid);
	pInode = fileMgr.NameI(FileManager::NextChar, FileManager::OPEN);
	if (NULL == pInode) // lookup failed
	{
		return;
	}

	/* Throttle concurrent exec */
	while (this->ExeCnt >= NEXEC)
	{
		u.u_procp->Sleep((unsigned long)&ExeCnt, ProcessManager::EXPRI);
	}
	this->ExeCnt++;

	/* Check execute permission */
	if (fileMgr.Access(pInode, Inode::IEXEC) || (pInode->i_mode & Inode::IFMT) != 0)
	{
		fileMgr.m_InodeTable->IPut(pInode);
		if (this->ExeCnt >= NEXEC)
		{
			WakeUpAll((unsigned long)&ExeCnt);
		}
		this->ExeCnt--;
		return;
	}

	PEParser parser;

	if (parser.HeaderLoad(pInode) == false)
	{
		fileMgr.m_InodeTable->IPut(pInode);
		return;
	}

	if (parser.TextSize + parser.DataSize + parser.StackSize + PageManager::PAGE_SIZE > MemoryDescriptor::USER_SPACE_SIZE - parser.TextAddress)
	{
		fileMgr.m_InodeTable->IPut(pInode);
		u.u_error = User::ENOMEM;
		return;
	}

	X86Assembly::CLI();    // atomic: allocate stack and map before releasing old memory

	int pages = (parser.StackSize + PageManager::PAGE_SIZE - 1) >> 12;
	int allocLength = pages << 12;   // stack allocation size in bytes

	unsigned long trueStack = (userPgMgr.AllocMemory(allocLength)) >> 12;  // allocate physical pages for user stack
	PageTableEntry *tempEntrys = (PageTableEntry *)Machine::Instance().GetUserPageTableArray();  // 0# user page table PTEs starting from index 1
	for (int i = 1; i <= pages; i++, trueStack++)
	{
		tempEntrys[i].m_UserSupervisor = 0x1;
		tempEntrys[i].m_Present = 0x1;
		tempEntrys[i].m_ReadWriter = true;
		tempEntrys[i].m_PageBaseAddress = trueStack;
	}
	trueStack -= pages;
	u.u_MemoryDescriptor.MapToPageTable();

	int argc = u.u_arg[1];  // command line argc
	char **argv = (char **)u.u_arg[2];

	/* esp points to top of stack */
	unsigned int esp = MemoryDescriptor::USER_SPACE_SIZE;
	unsigned long desAddress = 4096 + allocLength;

	int length;

	/* Copy argv[] strings onto user stack */
	for (int i = 0; i < argc; i++)
	{
		length = 0;
		while (NULL != argv[i][length])
		{
			length++;
		}
		desAddress = desAddress - (length + 1);
		Utility::MemCopy((unsigned long)argv[i], desAddress, length + 1);
		esp = esp - (length + 1);
		argv[i] = (char *)esp;
	}

	/* Align to 16-byte boundary */
	desAddress = desAddress & 0xFFFFFFF0;
	esp = esp & 0xFFFFFFF0;

	/* Push argc and argv[] */
	int endValue = 0;
	desAddress -= sizeof(endValue);
	esp -= sizeof(endValue);
	Utility::MemCopy((unsigned long)&endValue, desAddress, sizeof(endValue));

	desAddress -= argc * sizeof(int);
	esp -= argc * sizeof(int);
	Utility::MemCopy((unsigned long)argv, desAddress, argc * sizeof(int));

	endValue = esp;
	desAddress -= sizeof(int);
	esp -= sizeof(int);
	Utility::MemCopy((unsigned long)&endValue, desAddress, sizeof(int));

	desAddress -= sizeof(int);
	esp -= sizeof(int);
	Utility::MemCopy((unsigned long)&argc, desAddress, sizeof(int)); /* Done! */

	for (int i = 1; i <= pages; i++)  // unmap temporary mapping
	{
		tempEntrys[i].m_UserSupervisor = 0;
		tempEntrys[i].m_Present = 0;
		tempEntrys[i].m_ReadWriter = false;
		tempEntrys[i].m_PageBaseAddress = 1;
	}
	u.u_MemoryDescriptor.MapToPageTable();
	X86Assembly::STI();

	/* Free old text segment */
	if (u.u_procp->p_textp != NULL)
	{
		u.u_procp->p_textp->XFree();
		u.u_procp->p_textp = NULL;
	}

	Process *current = u.u_procp;

	PageTable *pUserPageTable = u.u_MemoryDescriptor.m_UserPageTableArray;
	MemoryDescriptor &md = u.u_MemoryDescriptor;

	// Free data segment pages
	int index = md.m_DataStartAddress >> 12 - 1024;
	int count = (md.m_DataSize + PageManager::PAGE_SIZE - 1) / PageManager::PAGE_SIZE;
	unsigned long frame;

	while (count)
	{
		frame = pUserPageTable->m_Entrys[index].m_PageBaseAddress;
		userPgMgr.FreeMemory(PageManager::PAGE_SIZE, frame << 12);

		pUserPageTable->m_Entrys[index].m_Present = 0;
		pUserPageTable->m_Entrys[index].m_ReadWriter = 0;
		pUserPageTable->m_Entrys[index].m_UserSupervisor = 1;
		pUserPageTable->m_Entrys[index].m_PageBaseAddress = 0;

		index++;		count--;
	}

	// Free stack segment pages
	index = (md.USER_SPACE_START_ADDRESS + md.USER_SPACE_SIZE - md.m_StackSize) >> 12 - 1024;
	count = (md.m_StackSize + PageManager::PAGE_SIZE - 1) / PageManager::PAGE_SIZE;
	while (count)
	{
		frame = pUserPageTable->m_Entrys[index].m_PageBaseAddress;
		userPgMgr.FreeMemory(PageManager::PAGE_SIZE, frame << 12);

		pUserPageTable->m_Entrys[index].m_Present = 0;
		pUserPageTable->m_Entrys[index].m_ReadWriter = 0;
		pUserPageTable->m_Entrys[index].m_UserSupervisor = 1;
		pUserPageTable->m_Entrys[index].m_PageBaseAddress = 0;

		index++;		count--;
	}

	/* Update MemoryDescriptor from PE header */
	u.u_MemoryDescriptor.m_TextStartAddress = parser.TextAddress;
	u.u_MemoryDescriptor.m_TextSize = parser.TextSize;
	u.u_MemoryDescriptor.m_DataStartAddress = parser.DataAddress;
	u.u_MemoryDescriptor.m_DataSize = parser.DataSize;
	u.u_MemoryDescriptor.m_StackSize = parser.StackSize;

	pText = NULL;
	/* Find or create Text structure, support text segment sharing */
	for (int i = 0; i < ProcessManager::NTEXT; i++)
	{
		if (NULL == this->text[i].x_iptr)
		{
			if (NULL == pText)
			{
				pText = &(this->text[i]);
			}
		}
		else if (pInode == this->text[i].x_iptr)
		{
			this->text[i].x_count++;
			this->text[i].x_ccount++;
			u.u_procp->p_textp = &(this->text[i]);
			pText = NULL;
			break;
		}
	}

	int sharedText = 0;

	PageTableEntry *entrys = (PageTableEntry *)md.m_UserPageTableArray;   // 1# user page table
	pages = (u.u_MemoryDescriptor.m_TextSize + 4096 - 1) / 4096;    // text segment pages
	int text_page_start = parser.TextAddress >> 12;  // first logical page of text
	text_page_start -= PageTable::ENTRY_CNT_PER_PAGETABLE;  // index into 1# user page table

	/* Initialize new Text structure: allocate physical pages discretely */
	if (NULL != pText)
	{
		pInode->i_count++;

		pText->x_ccount = 1;
		pText->x_count = 1;
		pText->x_iptr = pInode;
		pText->x_size = u.u_MemoryDescriptor.m_TextSize;

		unsigned long temp;
		for (int i = 0; i < pages; i++)
		{
			temp = userPgMgr.AllocMemory(M_PAGE_SIZE);  // allocate one physical page
			pText->x_caddr[i] = (temp) >> 12;  // store physical page frame number

			entrys[text_page_start + i].m_UserSupervisor = 0x1;
			entrys[text_page_start + i].m_Present = 0x1;
			entrys[text_page_start + i].m_ReadWriter = false;
			entrys[text_page_start + i].m_PageBaseAddress = pText->x_caddr[i];
		}

		pText->x_daddr = Kernel::Instance().GetSwapperManager().AllocSwap(pText->x_size);

		u.u_procp->p_textp = pText;
	}
	else
	{
		pText = u.u_procp->p_textp;
		for (int i = 0; i < pages; i++)  // map shared text pages
		{
			entrys[text_page_start + i].m_UserSupervisor = 0x1;
			entrys[text_page_start + i].m_Present = 0x1;
			entrys[text_page_start + i].m_ReadWriter = false;
			entrys[text_page_start + i].m_PageBaseAddress = pText->x_caddr[i];
		}
		sharedText = 1;
	}

	// Calculate new process size
	unsigned int newSize = ProcessManager::USIZE + u.u_MemoryDescriptor.m_DataSize + u.u_MemoryDescriptor.m_StackSize;
	u.u_procp->p_size = newSize;

	// Allocate data segment pages discretely
	pages = (u.u_MemoryDescriptor.m_DataSize + M_PAGE_SIZE - 1) / M_PAGE_SIZE;
	int data_page_start = (parser.DataAddress - md.USER_SPACE_START_ADDRESS) >> 12;
	data_page_start -= PageTable::ENTRY_CNT_PER_PAGETABLE;

	unsigned long temp;
	for (int i = 0; i < pages; i++)
	{
		temp = userPgMgr.AllocMemory(M_PAGE_SIZE);
		entrys[data_page_start + i].m_UserSupervisor = 0x1;
		entrys[data_page_start + i].m_Present = 0x1;
		entrys[data_page_start + i].m_ReadWriter = true;
		entrys[data_page_start + i].m_PageBaseAddress = temp >> 12;
	}

	// Map stack pages directly (already allocated above)
	pages = (u.u_MemoryDescriptor.m_StackSize + PageManager::PAGE_SIZE - 1) >> 12;
	unsigned long start = md.USER_SPACE_SIZE - allocLength;
	int stack_page = (start >> 12) - PageTable::ENTRY_CNT_PER_PAGETABLE;

	for (int i = 0; i < pages; i++, trueStack++)
	{
		entrys[stack_page + i].m_UserSupervisor = 0x1;
		entrys[stack_page + i].m_Present = 0x1;
		entrys[stack_page + i].m_ReadWriter = true;
		entrys[stack_page + i].m_PageBaseAddress = trueStack;
	}

	md.MapToPageTable();

	/* Load .text, .data, .rdata, .bss sections from exe file */
	parser.Relocate(pInode, sharedText);

	/* Write .text to swap area */
	if (sharedText == 0)
	{
		u.u_procp->p_flag |= Process::SLOCK;
		bufMgr.Swap(pText->x_daddr, pText->x_caddr[0], pText->x_size, Buf::B_WRITE);
		u.u_procp->p_flag &= ~Process::SLOCK;
	}

	/* Release Inode and decrement ExeCnt */
	fileMgr.m_InodeTable->IPut(pInode);
	if (this->ExeCnt >= NEXEC)
	{
		WakeUpAll((unsigned long)&ExeCnt);
	}
	this->ExeCnt--;

	/* Reset signals to default */
	for (int i = 0; i < u.NSIG; i++)
	{
		u.u_signal[i] = 0;
	}

	/* Zero general registers */
	for (int i = User::EAX - 4; i < User::EAX - 4 * 7; i = i - 4)
	{
		u.u_ar0[i] = 0;
	}

	/* Set entry point address into EAX for runtime() */
	u.u_ar0[User::EAX] = parser.EntryPointAddress;

	/* Set up context to return to ring3 */
	struct pt_context *pContext = (struct pt_context *)u.u_arg[4];
	pContext->eip = 0x00000000; /* ring3 entry - runtime() starts executing */
	pContext->xcs = Machine::USER_CODE_SEGMENT_SELECTOR;
	pContext->eflags = 0x200;
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
