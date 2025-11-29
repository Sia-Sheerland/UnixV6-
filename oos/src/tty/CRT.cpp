#include "CRT.h"
#include "IOPort.h"

unsigned short* CRT::m_VideoMemory = (unsigned short *)(0xB8000 + 0xC0000000);
unsigned int CRT::m_CursorX = 0;
unsigned int CRT::m_CursorY = 0;
char* CRT::m_Position = 0;
char* CRT::m_BeginChar = 0;

unsigned int CRT::ROWS = 15;

void CRT::CRTStart(TTy* pTTy)
{
	char ch;
	if ( 0 == CRT::m_BeginChar)
	{
		m_BeginChar = pTTy->t_outq.CurrentChar();
	}
	if ( 0 == m_Position )
	{
		m_Position = m_BeginChar;
	}

	while ( (ch = pTTy->t_outq.GetChar()) != TTy::GET_ERROR )
	{
		switch (ch)
		{
		case '\n':
			NextLine();
			CRT::m_BeginChar = pTTy->t_outq.CurrentChar();
			m_Position = CRT::m_BeginChar;
			break;

		case 0x15:
			//del_line();
			break;

		case '\b':
			if ( m_Position != CRT::m_BeginChar )
			{
				BackSpace();
				m_Position--;
			}
			break;

		case '\t':
			Tab();
			m_Position++;
			break;

		default:	/* ����Ļ�ϻ�����ͨ�ַ� */
			WriteChar(ch);
			m_Position++;
			break;
		}
   }
}

void CRT::MoveCursor(unsigned int col, unsigned int row)
{
	if ( (col < 0 || col >= CRT::COLUMNS) || (row < 0 || row >= CRT::ROWS) )
	{
		return;
	}

	/* ������ƫ���� */
	unsigned short cursorPosition = row * CRT::COLUMNS + col;

	/* ѡ�� r14��r15�Ĵ������ֱ�Ϊ���λ�õĸ�8λ�͵�8λ */
	IOPort::OutByte(CRT::VIDEO_ADDR_PORT, 14);
	IOPort::OutByte(CRT::VIDEO_DATA_PORT, cursorPosition >> 8);
	IOPort::OutByte(CRT::VIDEO_ADDR_PORT, 15);
	IOPort::OutByte(CRT::VIDEO_DATA_PORT, cursorPosition & 0xFF);
}

void CRT::NextLine()
{
	m_CursorX = 0;
	m_CursorY += 1;

	/* ��������������һ�еײ� */
	if ( m_CursorY >= CRT::ROWS )
	{
		m_CursorY = CRT::ROWS - 1;
		ScrollScreen();
	}
	MoveCursor(m_CursorX, m_CursorY);
}

void CRT::BackSpace()
{
	m_CursorX--;

	/* �ƶ���꣬���Ҫ�ص���һ�еĻ� */
	if ( m_CursorX < 0 )
	{
		m_CursorX = CRT::COLUMNS - 1;
		m_CursorY--;
		if ( m_CursorY < 0 )
		{
			m_CursorY = 0;
		}
	}
	MoveCursor(m_CursorX, m_CursorY);

	/* �ڹ������λ�����Ͽո� */
	m_VideoMemory[m_CursorY * COLUMNS + m_CursorX] = ' ' | CRT::COLOR;
	// m_VideoMemory[m_CursorY * COLUMNS + m_CursorX] = ' ' | 0x0A00;
}

void CRT::Tab()
{
	m_CursorX &= 0xFFFFFFF8;	/* ������뵽ǰһ��Tab�߽� */
	m_CursorX += 8;
	// const int TabWidth = 10;
	// m_CursorX -= m_CursorX % TabWidth;
	// m_CursorX += TabWidth;
	if ( m_CursorX >= CRT::COLUMNS )
		NextLine();
	else
		MoveCursor(m_CursorX, m_CursorY);
}

void CRT::WriteChar(char ch)
{
	m_VideoMemory[m_CursorY * CRT::COLUMNS + m_CursorX] = (unsigned char) ch | CRT::COLOR;
	m_CursorX++;
	
	if ( m_CursorX >= CRT::COLUMNS )
	{
		NextLine();
	}
    else
    {
    	MoveCursor(m_CursorX, m_CursorY);
    }
}

void CRT::ClearScreen()
{
	unsigned int i;

	for ( i = 0; i < COLUMNS * ROWS; i++ )
	{
		m_VideoMemory[i] = (unsigned short)' ' | CRT::COLOR;
	}
}

void CRT::ScrollScreen()
{
	unsigned int i;

	/* ����һ�е����ݸ��Ƶ���һ�� */
	for ( i = 0; i < COLUMNS * (ROWS - 1); i++ )
	{
		m_VideoMemory[i] = m_VideoMemory[i + COLUMNS];
	}

	/* ��������һ�� */
	for ( i = COLUMNS * (ROWS - 1); i < COLUMNS * ROWS; i++ )
	{
		m_VideoMemory[i] = (unsigned short)' ' | CRT::COLOR;
	}
}

