#ifndef	ASSEMBLY_H
#define	ASSEMBLY_H

/*
 * X86Assembly�ඨ���˶�x86ƽ̨�в�����Ȩ��ָ��ĳ���
 * 
 * ʹ��C++ Inline Assembly ��װlgdt, lidt, ltr, cli, 
 * sti��ָ����ڽ��C++�����޷�ͨ�������������Ȩ��
 * ָ���ɿ�/���жϡ�����gdt��idt������
*/

/* ˢ��ҳ������ÿ�ζ�ҳ�������޸ĺ���Ҫ���ã����»���ҳ�� */
/* NOTE 3: now accepts physical address of page directory */
#define FlushPageDirectory(phyAddr)	\
	__asm__ __volatile__(" movl %0, %%cr3" : : "r"(phyAddr) );

class X86Assembly
{
	public:
		//�����ж�
		static inline void STI()
		{
			__asm__ __volatile__("sti");
		}
		
		//�����ж�
		static inline void CLI()
		{
			__asm__ __volatile__("cli");
		}
		
		//lidtָ��
		static inline void LIDT(unsigned short idtr[3])
		{
			__asm__ __volatile__("lidt %0"::"m" (*idtr));
			//�ر����ѣ�lidtָ��Ĳ�������6�ֽڵ�Limit+BaseAddress, ��������6���ֽڵ��׵�ַ
			//���ԡ�(*idtr)���еġ� * ��������©��������
		}
		
		//lgdtָ��
		static inline void LGDT(unsigned short gdtr[3])
		{
			__asm__ __volatile__("lgdt %0"::"m" (*gdtr));
		}

		//ltrָ��
		static inline void LTR(unsigned short tssSelector)
		{
			__asm__ __volatile__("mov %0, %%ax\n\tltr %%ax"::"m"(tssSelector));
		}
};
#endif
