#include "CRT.h"
#include "IOPort.h"

unsigned short* CRT::m_VideoMemory = (unsigned short *)(0xB8000 + 0xC0000000);
unsigned int CRT::m_CursorX = 0;
unsigned int CRT::m_CursorY = 0;
char* CRT::m_Position = 0;
char* CRT::m_BeginChar = 0;

unsigned int CRT::ROWS = 15;

/* ��ʷ�������� */
static unsigned short g_HistoryBufferData[CRT::HISTORY_LINES * CRT::COLUMNS];
unsigned short* CRT::m_HistoryBuffer = g_HistoryBufferData;
unsigned int CRT::m_TotalLines = 0;
unsigned int CRT::m_ViewStartLine = 0;
bool CRT::m_AutoScroll = true;

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

	/* ���������ǰ�ܺ������������һ�� */
	if ( m_CursorY >= m_TotalLines )
	{
		/* ���ֻ����HISTORY_LINES������ʷ���в��� */
		if ( m_TotalLines < HISTORY_LINES )
		{
			m_TotalLines++;
		}
		else
		{
			/* ��ʷ�����������������ܷ��أ�*/
			ScrollScreen();
			m_CursorY = HISTORY_LINES - 1;
		}
	}

	/* �Զ���������ײ� */
	if ( m_AutoScroll && m_TotalLines > ROWS )
	{
		m_ViewStartLine = m_TotalLines - ROWS;
		RefreshScreen();
	}
	else if ( m_AutoScroll )
	{
		RefreshScreen();
	}

	MoveCursor(m_CursorX, m_CursorY - m_ViewStartLine);
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
	/* ȷ����ʷ�������С */
	if ( m_TotalLines == 0 )
	{
		m_TotalLines = ROWS;
		/* ��ʼ����ʷ������Ϊ�հ� */
		for ( unsigned int i = 0; i < HISTORY_LINES * COLUMNS; i++ )
		{
			m_HistoryBuffer[i] = (unsigned short)' ' | CRT::COLOR;
		}
	}

	/* д����ʷ���������ǰλ�� */
	unsigned int bufferPos = m_CursorY * COLUMNS + m_CursorX;
	m_HistoryBuffer[bufferPos] = (unsigned char) ch | CRT::COLOR;

	m_CursorX++;

	if ( m_CursorX >= CRT::COLUMNS )
	{
		NextLine();
	}
    else
    {
		/* ���AutoScroll����ˢ����һ���ַ� */
		if ( m_AutoScroll )
		{
			m_VideoMemory[(m_CursorY - m_ViewStartLine) * COLUMNS + m_CursorX - 1] = m_HistoryBuffer[bufferPos];
		}
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

	/* ����ʷ�����������һ�е����ݸ��Ƶ���һ�� */
	for ( i = 0; i < COLUMNS * (HISTORY_LINES - 1); i++ )
	{
		m_HistoryBuffer[i] = m_HistoryBuffer[i + COLUMNS];
	}

	/* ��������һ�� */
	for ( i = COLUMNS * (HISTORY_LINES - 1); i < COLUMNS * HISTORY_LINES; i++ )
	{
		m_HistoryBuffer[i] = (unsigned short)' ' | CRT::COLOR;
	}
}

void CRT::RefreshScreen()
{
	/* ����ʷ���������ݸ��Ƶ���Ļ */
	unsigned int displayLines = (m_TotalLines < ROWS) ? m_TotalLines : ROWS;

	for ( unsigned int row = 0; row < displayLines; row++ )
	{
		unsigned int historyRow = m_ViewStartLine + row;
		for ( unsigned int col = 0; col < COLUMNS; col++ )
		{
			m_VideoMemory[row * COLUMNS + col] = m_HistoryBuffer[historyRow * COLUMNS + col];
		}
	}

	/* �������ж����հ��� */
	for ( unsigned int row = displayLines; row < ROWS; row++ )
	{
		for ( unsigned int col = 0; col < COLUMNS; col++ )
		{
			m_VideoMemory[row * COLUMNS + col] = (unsigned short)' ' | CRT::COLOR;
		}
	}
}

void CRT::ScrollUp(unsigned int lines)
{
	/* �������ٻ� */
	if ( m_ViewStartLine >= lines )
	{
		m_ViewStartLine -= lines;
	}
	else
	{
		m_ViewStartLine = 0;
	}

	m_AutoScroll = false;
	RefreshScreen();

	/* �����������λ�� */
	unsigned int displayY = m_CursorY - m_ViewStartLine;
	if ( displayY < ROWS )
	{
		MoveCursor(m_CursorX, displayY);
	}
}

void CRT::ScrollDown(unsigned int lines)
{
	/* �������¼�������� */
	unsigned int maxStartLine = (m_TotalLines > ROWS) ? (m_TotalLines - ROWS) : 0;

	if ( m_ViewStartLine + lines <= maxStartLine )
	{
		m_ViewStartLine += lines;
	}
	else
	{
		m_ViewStartLine = maxStartLine;
		m_AutoScroll = true;	/* �������ײ��Զ����� */
	}

	RefreshScreen();

	/* �����������λ�� */
	unsigned int displayY = m_CursorY - m_ViewStartLine;
	if ( displayY < ROWS )
	{
		MoveCursor(m_CursorX, displayY);
	}
}

