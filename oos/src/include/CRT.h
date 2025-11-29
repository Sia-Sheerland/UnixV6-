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

	/* ��ʷ������������� */
	static const unsigned int HISTORY_LINES = 100;	/* �������ж��ٻ���ʷ�� */

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

	/* ��ͼ���ڹ��� - ��������ʷ���� */
	static void ScrollUp(unsigned int lines = 1);

	/* ��ͼ���ڹ��� - ���¹���������� */
	static void ScrollDown(unsigned int lines = 1);

	/* ˢ����ʾ - ����ʷ���������ݵ���Ļ */
	static void RefreshScreen();

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

	/* ��ʷ�������� */
	static unsigned short* m_HistoryBuffer;			/* ��ʷ�������� */
	static unsigned int m_TotalLines;				/* ��ǰ�ܹ������ж��ٻ� */
	static unsigned int m_ViewStartLine;			/* ��ǰ��ͼ���ڵ���ʼ�к� */
	static bool m_AutoScroll;						/* �Ƿ��Զ������������ */
};

#endif
