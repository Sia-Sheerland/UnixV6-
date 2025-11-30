#ifndef CHIP8259A_H
#define CHIP8259A_H

/*
 * �����˶�8259A�ɱ��жϿ���оƬ(PIC)�Ĳ�����
 * 
 * 8259AоƬ����CPU��Ϊ��������������ж�����,
 * ѡ�����ȼ���ߵ��ж�,ת���CPU����Ӧ�жϡ�
 */

class Chip8259A
{
public:
	/* ��ʼ��ϵͳ�е���������Ƭ8259A�жϿ���оƬ */
	static void Init();
	
	/* �����жϺ�����
	 *    
	 * ���ܣ�ͨ������8259A���ж����μĴ���������Ӧ
	 * �ж�����λ��0�����������ض�������жϡ�
	 * 
	 * ����������ض������IRQ�š������涨���: IRQ_TIMER = 0;��
	 */
	static void IrqEnable(unsigned int irq);
	
	/* �����жϺ����� ��IrqEnable(unsigned int irq)ִ�з����ܡ�
	 * 
	 * ���ܣ�ͨ������8259A���ж����μĴ���������Ӧ
	 * �ж�����λ��1�����������ض�������жϡ�
	 * 
	 * ����������ض������IRQ�š������涨���: IRQ_TIMER = 0; ��
	 */
	static void IrqDisable(unsigned int irq);
	
public:
	/* ϵͳ����2Ƭ8259AоƬ��ÿһƬ��IO��ַ�ռ���ռ��2���˿ڵ�ַ */
	/* ��Ƭ(Master)��IO�˿ڵ�ַ */
	static const unsigned short MASTER_IO_PORT_1 = 0x20;
	static const unsigned short MASTER_IO_PORT_2 = 0x21;
	
	/* ��Ƭ(Slave)��IO�˿ڵ�ַ */
	static const unsigned short SLAVE_IO_PORT_1 = 0xA0;
	static const unsigned short SLAVE_IO_PORT_2 = 0xA1;
	
	/* ��Ƭ�����Ŷ�Ӧ��ʼ�жϺ� */
	static const unsigned char MASTER_IRQ_START = 0x20;
	/* ��Ƭ�����Ŷ�Ӧ��ʼ�жϺ�,�жϺŷ�Χ��ʼ0x28 */
	static const unsigned char SLAVE_IRQ_START = MASTER_IRQ_START + 8;	
	
	/* 
	 * ��Ƭ(IR0~IR7)���ӵ��������Ӧ���ж��������� (����ֻ�����ں���
	 * �õ�������)���������ѡ���Ե� ����/���������ڸ����ŵ��жϡ�
	 */
	static const unsigned int IRQ_TIMER = 0;	/* ʱ���ж�(IRQ0)���͵�IR0���� */
	static const unsigned int IRQ_KBD	= 1;	/* �����ж�(IRQ1)���͵�IR1���� */
	static const unsigned int IRQ_SLAVE = 2;	/* ����ģʽ��,��Ƭ�������ж�(Slave��INT����),���͵���Ƭ��IR2 */

	/* ��Ƭ(IR0~IR7)���ӵ��������Ӧ���ж���������,����ֻ�õ���������Ӳ�� */
	static const unsigned int IRQ_MOUSE	= 12;	/* 鼠标中断(IRQ12)连接到从片IR4引脚 */
	static const unsigned int IRQ_IDE	= 14;	/* Ӳ���ж�(IRQ14)���͵���ƬIR6���� */
	
	/* ������Ҫ�����һЩ���� */
	static const unsigned char MASK_ALL = 0xFF;	/* ����״̬��, ������������(IR0~IR7)�ϵ��ж����� */
	static const unsigned char EOI = 0x20;		/* End Of Interrupt */
};

#endif
