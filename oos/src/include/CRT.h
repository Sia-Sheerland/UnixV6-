#ifndef CRT_H
#define CRT_H

#include "TTy.h"

class CRT
{
	/* Const Member */
public:
	/* ��ʾ���ƼĴ���I/O�˿ڵ�ַ */
	static const unsigned short VIDEO_ADDR_PORT = 0x3d4;	/* ��ʾ���������Ĵ����˿ں� */
	static const unsigned short VIDEO_DATA_PORT = 0x3d5;	/* ��ʾ�������ݼĴ����˿ں� */
	
	/* ��Ļ��СΪ80 * 25 */
	static const unsigned int COLUMNS = 80;
	static unsigned int ROWS;
	
	static const unsigned short COLOR = 0x0F00;		/* char in white color */

	/* Functions */
public:
	/* �������������е������������Ļ�� */
	static void CRTStart(TTy* pTTy);

	/* �ı���λ�� */
	static void MoveCursor(unsigned int x, unsigned int y);

	/* ���д����ӳ��� */
	static void NextLine();

	/* �˸����ӳ��� */
	static void BackSpace();
	
	/* Tab�����ӳ��� */
	static void Tab();

	/* ��ʾ�����ַ� */
	static void WriteChar(char ch);

	/* �����Ļ */
	static void ClearScreen();

	/* ��Ļ�������һ�� */
	static void ScrollScreen();

	/* Members */
public:
	static unsigned short* m_VideoMemory;
	static unsigned int m_CursorX;
	static unsigned int m_CursorY;

	/* ָ�������������е�ǰҪ������ַ� */
	static char* m_Position;
	/* ָ��������������δȷ�ϵ�����ַ��Ŀ�ʼ����������ͨ��Backspace��ɾ�������ݣ�
	 * ���һ���س�֮ǰ������Ϊ��ȷ�����ݣ����ɱ�Backspace��ɾ����
	 */
	static char* m_BeginChar;
};

#endif
