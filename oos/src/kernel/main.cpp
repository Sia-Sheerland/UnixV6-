/* �ں˵ĳ�ʼ�� */

#include "Video.h"
#include "Simple.h"
#include "IOPort.h"
#include "Chip8253.h"
#include "Chip8259A.h"
#include "Machine.h"
#include "IDT.h"
#include "Assembly.h"
#include "Kernel.h"
#include "TaskStateSegment.h"

#include "PageDirectory.h"
#include "PageTable.h"
#include "SystemCall.h"

#include "Exception.h"
#include "DMA.h"
#include "CRT.h"
#include "TimeInterrupt.h"
#include "PEParser.h"
#include "CMOSTime.h"
#include "Mouse.h"
#include "..\test\TestInclude.h"

bool isInit = false;

extern "C" void MasterIRQ7()
{
	SaveContext();
	
	Diagnose::Write("IRQ7 from Master 8259A!\n");
	
	//��Ҫ���жϴ�������ĩβ��8259A����EOI����
	//ʵ�鷢�֣���û������IOPort::OutByte(0x27, 0x20);�������Ч����һ����������Ϊ
	//����EOI����֮����к�����IRQ7�жϽ��룬 �������������IRQ7ֻ�����һ�Ρ�
	IOPort::OutByte(Chip8259A::MASTER_IO_PORT_1, Chip8259A::EOI);

	RestoreContext();

	Leave();

	InterruptReturn();
}

extern "C" int main0(void)
{
	Machine& machine = Machine::Instance();

	Chip8259A::Init();
	Chip8259A::IrqEnable(Chip8259A::IRQ_SLAVE);

	Chip8253::Init(60);	//��ʼ��ʱ���ж�оƬ
	Chip8259A::IrqEnable(Chip8259A::IRQ_TIMER);

	Chip8259A::IrqEnable(Chip8259A::IRQ_KBD);

	DMA::Init();
	Chip8259A::IrqEnable(Chip8259A::IRQ_IDE);

	/* 初始化鼠标驱动 */
	Mouse::Initialize();
	Chip8259A::IrqEnable(Chip8259A::IRQ_MOUSE);

	// Chip8253::Init(20);	//��ʼ��ʱ���ж�оƬ
	// Chip8259A::Init();
	// Chip8259A::IrqEnable(Chip8259A::IRQ_TIMER);
	// DMA::Init();
	// Chip8259A::IrqEnable(Chip8259A::IRQ_IDE);
	// Chip8259A::IrqEnable(Chip8259A::IRQ_SLAVE);
	// Chip8259A::IrqEnable(Chip8259A::IRQ_KBD);


	//init gdt
	machine.InitGDT();
	machine.LoadGDT();
	//init idt
	machine.InitIDT();	
	machine.LoadIDT();

	machine.InitPageDirectory();    // ��ʼ��ҳĿ¼������̬ҳ��
	machine.InitUserPageTable();     // ��ʼ���û�̬ҳ��
	machine.EnablePageProtection();    //������ҳģʽ

	/* ���ϣ�InitUserPageTable()�����Ե�ַ0-8Mӳ�䵽�����ڴ�
	 * ����0-4M��Ϊ��֤��ע����������������β�Ĵ�����ȷִ�У�
	 *
	 * ������ΪĿǰ������CS���ں˳�ʼ���׶εĶ�ѡ���ӣ�����μĴ���ȫ��bootʹ�õĶ�ѡ���ӣ�������SS��
	 * �ֶε�Ԫ���������Ե�ַ��[0,4M)��������ҳģʽ��һ��Ҫ����οռ��ӳ���ϵ������ͨ������������ԣ���C����EnablePageProtection()�ĵľֲ������ͷ��ص�ַС��4M��û��ӳ���δ��󣩡�
	 */

	//ʹ��0x10�μĴ���
	__asm__ __volatile__
		(" \
		mov $0x10, %ax\n\t \
		mov %ax, %ds\n\t \
		mov %ax, %ss\n\t \
		mov %ax, %es\n\t"
		);

	//����ʼ����ջ����Ϊ0xc0400000�������ƻ��˷�װ�ԣ�����ʹ�ø��õķ���
	__asm__ __volatile__
		(
		" \
		mov $0xc0400000, %ebp \n\t \
		mov $0xc0400000, %esp \n\t \
		jmp $0x8, $_next"
		);
	
}

/* Ӧ�ó����main���أ����̾���ֹ�ˣ���ȫ��runtime()�Ĺ��͡�û��������ֻ����exit��ֹ�����ˡ�xV6û�������^-^ */
extern "C" void runtime()
{
	/*
	1. ����runtime��stack Frame
	2. esp��ָ���û�ջ��argcλ�ã���ebp��δ��ȷ��ʼ��
	3. eax�д�ſ�ִ�г���EntryPoint
	4~6. exit(0)��������
	*/
	__asm__ __volatile__("	leave;	\
							movl %%esp, %%ebp;	\
							call *%%eax;		\
							movl $1, %%eax;	\
							movl $0, %%ebx;	\
							int $0x80"::);
}

/*
  * 1#������ִ����MoveToUserStack()��ring0�˳���ring3���ȼ��󣬻����ExecShell()���˺���ͨ��"int $0x80"
  * (EAX=execvϵͳ���ú�)���ء�/Shell.exe�������书���൱�����û�������ִ��ϵͳ����execv(char* pathname, char* argv[])��
  */
extern "C" void ExecShell()
{
	int argc = 0;
	char* argv = NULL;
	char* pathname = "/Shell.exe";
	__asm__ __volatile__ ("int $0x80"::"a"(11/* execv */),"b"(pathname),"c"(argc),"d"(argv));
	return;
}

/* �˺���test�ļ����еĴ�������ã���ò�ƿ���ɾ�����ǵð���ɾ��*/
extern "C" void Delay()
{
	for ( int i = 0; i < 50; i++ )
		for ( int j = 0; j < 10000; j++ )
		{
			int a;
			int b;
			int c=a+b;
			c++;
		}
}

extern "C" void next()
{
	//���ʱ��0M-4M���ڴ�ӳ���Ѿ�����ʹ���ˣ�����Ҫ����ӳ���û�̬��ҳ����Ϊ�û�̬������������׼��
	//Machine::Instance().InitUserPageTable();
	//FlushPageDirectory();

	Machine::Instance().LoadTaskRegister();
	
	/* ��ȡCMOS��ǰʱ�䣬����ϵͳʱ�� */
	struct SystemTime cTime;
	CMOSTime::ReadCMOSTime(&cTime);
	/* MakeKernelTime()������ں�ʱ�䣬��1970��1��1��0ʱ����ǰ������ */
	Time::time = Utility::MakeKernelTime(&cTime);

	/* ��CMOS�л�ȡ�����ڴ��С */
	unsigned short memSize = 0;	/* size in KB */
	unsigned char lowMem, highMem;

	/* ����ֻ�ǽ���CMOSTime���е�ReadCMOSByte������ȡCMOS�������ڴ��С��Ϣ */
	lowMem = CMOSTime::ReadCMOSByte(CMOSTime::EXTENDED_MEMORY_ABOVE_1MB_LOW);
	highMem = CMOSTime::ReadCMOSByte(CMOSTime::EXTENDED_MEMORY_ABOVE_1MB_HIGH);
	memSize = (highMem << 8) + lowMem;

	/* ����1MB���������ڴ����򣬼������ڴ����������ֽ�Ϊ��λ���ڴ��С */
	memSize += 1024; /* KB */
	PageManager::PHY_MEM_SIZE = memSize * 1024;
	UserPageManager::USER_PAGE_POOL_SIZE = PageManager::PHY_MEM_SIZE - UserPageManager::USER_PAGE_POOL_START_ADDR;

	/* ��������ϵͳ�ں˳�ʼ���߼�	 */
	Kernel::Instance().Initialize();	
	Kernel::Instance().GetProcessManager().SetupProcessZero();
	isInit = true;

	Kernel::Instance().GetFileSystem().LoadSuperBlock();
	Diagnose::Write("Unix V6++ FileSystem Loaded......OK\n");

	Diagnose::Write("test \n");

	/*  ��ʼ��rootDirInode���û���ǰ����Ŀ¼���Ա�NameI()�������� */
	FileManager& fileMgr = Kernel::Instance().GetFileManager();

	//fileMgr.rootDirInode = g_InodeTable.IGet(DeviceManager::ROOTDEV, FileSystem::ROOTINO);
	fileMgr.rootDirInode = g_InodeTable.IGet(DeviceManager::ROOTDEV, 1);
	fileMgr.rootDirInode->i_flag &= (~Inode::ILOCK);

	User& us = Kernel::Instance().GetUser();
	us.u_cdir = g_InodeTable.IGet(DeviceManager::ROOTDEV, 1);
	//us.u_cdir = g_InodeTable.IGet(DeviceManager::ROOTDEV, FileSystem::ROOTINO);
	us.u_cdir->i_flag &= (~Inode::ILOCK);
	Utility::StringCopy("/", us.u_curdir);

	/* ��TTy�豸 */
	int fd_tty = lib_open("/dev/tty1", File::FREAD);

	if ( fd_tty != 0 )
	{
		Utility::Panic("STDIN Error!");
	}
	fd_tty = lib_open("/dev/tty1", File::FWRITE);
	if ( fd_tty != 1 )
	{
		Utility::Panic("STDOUT Error!");
	}
	Diagnose::TraceOn();


	unsigned char* runtimeSrc = (unsigned char*)runtime;
	unsigned char* runtimeDst = 0x00000000;
	for (unsigned int i = 0; i < (unsigned long)ExecShell - (unsigned long)runtime; i++)
	{
		*runtimeDst++ = *runtimeSrc++;
	}

    //us.u_MemoryDescriptor.Release();

	int pid = Kernel::Instance().GetProcessManager().NewProc();         /* 0#���̴���1#���� */
	if( 0 == pid )     /* 0#����ִ��Sched()����Ϊϵͳ����Զ�����ں���̬��Ψһ����  */
	{
		us.u_procp->p_ttyp = NULL;
		Kernel::Instance().GetProcessManager().Sched();
	}
	else               /* 1#����ִ��Ӧ�ó���shell.exe,����ͨ����  */
	{
		Machine::Instance().InitUserPageTable();      //����ֱ��д0x202,0x203ҳ����û�����ʵ��ַӳ���һ��okay��
		FlushPageDirectory();

		CRT::ClearScreen();

		/* 1#���̻��û�̬��ִ��exec("shell.exe")ϵͳ����*/
		MoveToUserStack();
		__asm__ __volatile__ ("call *%%eax" :: "a"((unsigned long)ExecShell - 0xC0000000));   //Ҫ�����û�ջ������һ��Ҫ��ӳ�䣡
	}
}



