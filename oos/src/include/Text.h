#ifndef TEXT_H
#define TEXT_H

#include "INode.h"

/*
 * @comment ��ӦUnixv6�� struct text�ṹ
 * ������ִ�д������Ķ�(code segment)����Ϣ
 *
 *
 */
class Text
{
public:
	Text();
	~Text();

	/* �ݼ�x_ccount��ֵ�����x_ccount�ݼ���0��
	 * ��ʾ�ڴ����Ѿ�û�����øù������ĶεĽ��̣�
	 * ���ͷŸù������Ķ�ռ�ݵ��ڴ档
	 */
	void XccDec();

	/*
	 * �����ͷ������õĹ������ĶΣ�ͨ��������Exit()��Exec()ʱ��
	 */
	void XFree();

public:
	int x_daddr;			 /* �������Ķ����̽������ϵĵ�ַ */
	unsigned long x_caddr[10];	 /* ��������Ķε�����ҳ��š����Ķ�ֻ����10ҳ��Ҫ�� */
	unsigned int x_size;	 /* ����γ��ȣ����ֽ�Ϊ��λ */
	Inode*			x_iptr;		/* �ڴ�inode��ַ */
	unsigned short	x_count;	/* �������ĶεĽ����� */
	unsigned short	x_ccount;	/* ���������Ķ���ͼ�����ڴ�Ľ����� */	
};

#endif

