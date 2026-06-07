#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include "Process.h"
#include "Assembly.h"

class User;

/* 
 * ����esp��ebp��u�ṹ�ĺ꣬������Ҫ�������NewProc()��Swtch()������
 * ���������ֻ��ʹ�úꡣ���򣬷��ص�ַ�����SaveU()ʱ�ĵ�ַ��������
 * ���ص�SaveU()��������һ��ִ�С��ý���ǲ���ȷ��
 */
 /*
#define SaveU(u) \
	__asm__ __volatile__(	\
		"movl %%esp, %0\n\t \
		 movl %%ebp, %1\n\t" \
		:"+m"((u).u_rsav[0]),"+m"((u).u_rsav[1]) \
		); 
*/

#define SaveU(u_sav) \
	__asm__ __volatile__(	\
		"movl %%esp, %0\n\t \
		 movl %%ebp, %1\n\t" \
		:"+m"(u_sav[0]),"+m"(u_sav[1]) \
		); 
		
/* 
 * ˢ���ں�ҳ����1023��ĺ꣬�ڽ��̵���ʱ��ָ��ָ�����̵�u����ַ��ʹ
 * ��GetUser()�������ص�ǰ���̵�u�ṹ
 */
#define SwtchUStruct(p) \
	Machine::Instance().GetKernelPageTable().m_Entrys[Kernel::USER_PAGE_INDEX].m_PageBaseAddress \
		= (p)->p_addr / PageManager::PAGE_SIZE; \
	FlushPageDirectory();

/* 
 * �ָ�esp��ebp��u�ṹ�ĺ꣬ʹ�ú������ͬSaveU()
 */
/* #define RetU(u) \
	__asm__ __volatile__( \
		"movl %0, %%ebp\n\t \
		 movl %1, %%esp\n\t" \
		: \
		:"m"((u).u_rsav[1]),"m"((u).u_rsav[0]) \
		);	\
	FlushPageDirectory(); 
 */

#define RetU()	\
	__asm__ __volatile__("	movl %0, %%eax;				\
							movl (%%eax), %%esp;		\
							movl 0x4(%%eax), %%ebp;"	\
							:							\
							: "i" (0xc03ff000) );

#define aRetU(u_sav) \
	__asm__ __volatile__("	movl %0, %%esp;			\
							movl %1, %%ebp;"		\
							:						\
							:"m"(u_sav[0]), "m"(u_sav[1]) );
	
class ProcessManager
{
	/* static consts */
public:
	static const int NPROC = 100;
	static const unsigned long PROCESS_ZERO_PPDA_ADDRESS = 0x400000 - 0x1000;

	static const int NTEXT = 50;

	static const int NEXEC = 10;

	static const unsigned int USIZE = 0x1000;	/* ppda����С���ֽ�Ϊ��λ */

	/* 
	 * ���̽���˯��״̬ʱ���ں˸�����˯��ԭ�����������������������
	 * ������С����Ϊ������Ȩ˯�ߣ�������������Ϊ������Ȩ˯�ߡ�
	 */
	static const int PSWP = -100;
	static const int PINOD = -90;
	static const int PRIBIO = -50;
	static const int EXPRI = -1;
	static const int PPIPE = 1;
	static const int TTIPRI = 10;
	static const int TTOPRI = 20;
	static const int PWAIT = 40;
	static const int PSLEP = 90;
	static const int PUSER = 100;

	/* Functions */
public:
	/* Constructors */
	ProcessManager();
	/* Destructors */
	~ProcessManager();

	void Initialize();

	/* �ֹ�����ϵͳ0#���̣��������̶���ͨ��fork()����0#�������� */
	void SetupProcessZero();

	/*
	 * Swtch()���м�0#������̨�󣬵��ô˺���ѡ��
	 * ���ʺ���̨���еĽ���
	 */
	Process* Select();

	/*
	 * @comment �������ɵ�ǰ�������н��̵Ŀ�������ӡ�����̷���ֵΪ0.
	 * �ӽ����ڱ����Ⱥ󷵻�1,�����������ָ��ӽ��̵�Ψһ��������������
	 * ���������,���ӽ�����Ҫ�����Ⱥ�����
	 *
	 */
	int NewProc();

	int Swtch();

	/* 
	 * ����ͼ���ڴ�ͽ�����֮��Ĵ��͡�����н�����Ҫ�����ڴ棬���ڴ�
	 * ���޷��ҵ��ܹ����ɸý��̵������ڴ����������ν�������Ȩ˯��״̬(SWAIT)-->
	 * ��ͣ״̬(SSTOP)-->������Ȩ˯��״̬(SSLEEP)-->����״̬(SRUN)���̻�����
	 * ֱ���ڳ��㹻�ڴ�ռ佫��Ҫ����Ľ��̵����ڴ�
	 */
	void Sched();

	/* 
	 * �����̵ȴ��ӽ��̽�����Wait()ϵͳ����
	 */
	void Wait();

	/* 
	* ���̴���Fork()ϵͳ����
	*/
	void Fork();

	/* 
	 * @comment Exec()ϵͳ���ã�����ͼ��Ļ�
	 */
	void Exec();

	/* 
	 * ��ֹ����Kill()ϵͳ����
	 */
	void Kill();

	/*
	 * ����ϵͳ��������chan������˯�ߵĽ���
	 */
	void WakeUpAll(unsigned long chan);

	/*
	 * �����̴��ڴ滻�������̽�������
	 * pProcess: ָ��Ҫ�����Ľ���
	 * bFreeMemory: �Ƿ��ͷŽ���ͼ��ռ�ݵ��ڴ�
	 * size: ���������Ķ��⣬���̿ɽ�������ͼ�񳤶ȣ�����sizeΪ0ʱ��ֱ��ʹ��p_size
	 */
	void XSwap(Process* pProcess, bool bFreeMemory, int size);

	/*
	 * ���ź�signal�������뷢�ͽ�������ͬһ�ն˵����н���
	 */
	void Signal(TTy* pTTy, int signal);

	/* 释放当前进程在 shadow 页表中逐页分配的所有用户物理页 */
	static void FreeUserPages(User& u);

	/* Members */
public:
	Process process[NPROC];
	Text text[NTEXT];

	int CurPri;		/* ������ռ��CPUʱ������ */
	int RunRun;		/* ǿ�ȵ��ȱ�־ */
	int RunIn;		/* �ڴ����޺��ʽ��̿��Ե������̽����� */
	int RunOut;		/* �̽��������޽��̿��Ե����ڴ� */
	int ExeCnt;		/* ͬʱ����ͼ��Ļ��Ľ����� */
	int SwtchNum;	/* ϵͳ�н����л����� */

private:
	static unsigned int m_NextUniquePid;
public:
	static unsigned int NextUniquePid();

};

#endif

