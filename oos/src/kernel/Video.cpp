#include "Video.h"

unsigned short* Diagnose::m_VideoMemory = (unsigned short *)(0xB8000 + 0xC0000000);
unsigned int Diagnose::m_Row = 10;
unsigned int Diagnose::m_Column = 0;

unsigned int Diagnose::ROWS = 10;

bool Diagnose::trace_on = true;

/* ��ʷ�������� */
static unsigned short g_DiagnoseHistoryBuffer[Diagnose::HISTORY_LINES * Diagnose::COLUMNS];
unsigned short* Diagnose::m_HistoryBuffer = g_DiagnoseHistoryBuffer;
unsigned int Diagnose::m_TotalLines = 0;
unsigned int Diagnose::m_ViewStartLine = 0;
bool Diagnose::m_AutoScroll = true;

Diagnose::Diagnose()
{
	//ȫ������static��Ա����������û��ʲô��Ҫ�ڹ��캯���г�ʼ���ġ�
}

Diagnose::~Diagnose()
{
	//this is an empty dtor
}

void Diagnose::TraceOn()
{
	Diagnose::trace_on = 1;
}

void Diagnose::TraceOff()
{
	Diagnose::trace_on = 0;
}

/*
	�ܹ������ʽ������ַ�����Ŀǰֻ��ʶ��һЩ%d %x  %s ��%n;
	û�м������ܣ�% �� ֵƥ��Ҫ�Լ�ע�⡣
*/
void Diagnose::Write(const char* fmt, ...)
{
	if ( false == Diagnose::trace_on )
	{
		return;
	}
	//ʹva_arg�д�Ų���fmt�� ����һ�������� ���ڵ��ڴ��ַ
	//fmt�����ݱ������ַ������׵�ַ(�ⲻ������Ҫ��)����&fmt + 1������һ�������ĵ�ַ
	//�ο�UNIX v6�еĺ���prf.c/printf(fmt, x1,x2,x3,x4,x5,x6,x7,x8,x9,xa,xb,xc)
	unsigned int * va_arg = (unsigned int *)&fmt + 1;
	const char * ch = fmt;
	
	while(1)
	{
		while(*ch != '%' && *ch != '\n')
		{
			if(*ch == '\0')
				return;
			if(*ch == '\n')
				break;
			/*ע�⣺ '\n'��һ����һ�ַ���������'\\'�� ��n'�����ַ�����ӣ� 
			Ʃ�����ַ���"\nHello World!!"������Ƚ� if(*ch == '\\' && *(ch+1) == '\n' ) �Ļ���
			�����ĺݲҵģ�*/
			WriteChar(*ch++);
		}
		
		ch++;	//skip the '%' or '\n'   

		if(*ch == 'd' || *ch == 'x')
		{//%d �� %x ��ʽ���������ȻҪ���Ӱ˽��ƺͶ�����Ҳ�����ף����ô�����
			int value = (int)(*va_arg);
			va_arg++;
			if(*ch == 'x')
				Write("0x");   //as prefix for HEX value
			PrintInt(value, *ch == 'd' ? 10 : 16);
			ch++;	//skip the 'd' or 'x'
		}
		
		else if(*ch == 's')
		{//%s ��ʽ�����
			ch++;	//skip the 's'
			char *str = (char *)(*va_arg);
			va_arg++;
			while(char tmp = *str++)
			{
				WriteChar(tmp);
			}
		}
		else /* if(*(ch-1) == '\n') */
		{
			Diagnose::NextLine();
		}
	}
}

/*
	�ο�UNIX v6�еĺ���prf.c/printn(n,b)
	�˺����Ĺ����ǽ�һ��ֵvalue��base���Ƶķ�ʽ��ʾ������
*/
void Diagnose::PrintInt(unsigned int value, int base)
{
	//��Ϊ����0��9 �� A~F��ASCII��֮�䲻�������ģ����Բ��ܼ�ͨ��
	//ASCII(i) = i + '0'ֱ�Ӽ���õ����������Digits�ַ����顣
	static char Digits[] = "0123456789ABCDEF";
	int i;
	
	if((i = value / base) != 0)
		PrintInt(i ,base);
	WriteChar(Digits[value % base]);
}

void Diagnose::NextLine()
{
	m_Row += 1;
	m_Column = 0;

	/* ������������� */
	if ( m_Row >= m_TotalLines )
	{
		if ( m_TotalLines < HISTORY_LINES )
		{
			m_TotalLines++;
		}
		else
		{
			ScrollScreen();
			m_Row = HISTORY_LINES - 1;
		}
	}

	/* �Զ��������� */
	if ( m_AutoScroll && m_TotalLines > ROWS )
	{
		m_ViewStartLine = m_TotalLines - ROWS;
		RefreshScreen();
	}
	else if ( m_AutoScroll )
	{
		RefreshScreen();
	}
}

void Diagnose::WriteChar(const char ch)
{
	/* ��ʼ����ʷ���� */
	if ( m_TotalLines == 0 )
	{
		m_TotalLines = ROWS;
		m_Row = 0;
		for ( unsigned int i = 0; i < HISTORY_LINES * COLUMNS; i++ )
		{
			m_HistoryBuffer[i] = (unsigned short)' ' | Diagnose::COLOR;
		}
	}

	/* �����Զ������ر�����������������Զ���������ײ� */
	if ( !m_AutoScroll )
	{
		m_AutoScroll = true;
		if ( m_TotalLines > ROWS )
		{
			m_ViewStartLine = m_TotalLines - ROWS;
		}
		else
		{
			m_ViewStartLine = 0;
		}
		RefreshScreen();
	}

	if(Diagnose::m_Column >= Diagnose::COLUMNS)
	{
		NextLine();
	}

	/* д����ʷ���� */
	unsigned int bufferPos = m_Row * COLUMNS + m_Column;
	m_HistoryBuffer[bufferPos] = (unsigned char) ch | Diagnose::COLOR;

	/* ��ʾ����ʾ�ڴ� */
	if ( m_Row >= m_ViewStartLine && m_Row < m_ViewStartLine + ROWS )
	{
		unsigned int screenRow = m_Row - m_ViewStartLine + (SCREEN_ROWS - ROWS);
		Diagnose::m_VideoMemory[screenRow * COLUMNS + m_Column] = m_HistoryBuffer[bufferPos];
	}

	Diagnose::m_Column++;
}

void Diagnose::ClearScreen()
{
	unsigned int i;

	Diagnose::m_Row = Diagnose::SCREEN_ROWS - Diagnose::ROWS;
	Diagnose::m_Column = 0;

	for(i = 0; i < (COLUMNS * ROWS); i++)
	{
		Diagnose::m_VideoMemory[i + m_Row * COLUMNS] = (unsigned char) ' ' | Diagnose::COLOR;
	}
}

void Diagnose::ScrollScreen()
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
		m_HistoryBuffer[i] = (unsigned short)' ' | Diagnose::COLOR;
	}
}

void Diagnose::RefreshScreen()
{
	unsigned int startRow = SCREEN_ROWS - ROWS;
	unsigned int displayLines = (m_TotalLines < ROWS) ? m_TotalLines : ROWS;

	/* ����ʷ�������ݸ��Ƶ���Ļ */
	for ( unsigned int row = 0; row < displayLines; row++ )
	{
		unsigned int historyRow = m_ViewStartLine + row;
		for ( unsigned int col = 0; col < COLUMNS; col++ )
		{
			m_VideoMemory[(startRow + row) * COLUMNS + col] = m_HistoryBuffer[historyRow * COLUMNS + col];
		}
	}

	/* �������ж����հ��� */
	for ( unsigned int row = displayLines; row < ROWS; row++ )
	{
		for ( unsigned int col = 0; col < COLUMNS; col++ )
		{
			m_VideoMemory[(startRow + row) * COLUMNS + col] = (unsigned short)' ' | Diagnose::COLOR;
		}
	}
}

void Diagnose::ScrollUp(unsigned int lines)
{
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
}

void Diagnose::ScrollDown(unsigned int lines)
{
	unsigned int maxStartLine = (m_TotalLines > ROWS) ? (m_TotalLines - ROWS) : 0;

	if ( m_ViewStartLine + lines <= maxStartLine )
	{
		m_ViewStartLine += lines;
	}
	else
	{
		m_ViewStartLine = maxStartLine;
		m_AutoScroll = true;
	}

	RefreshScreen();
}
