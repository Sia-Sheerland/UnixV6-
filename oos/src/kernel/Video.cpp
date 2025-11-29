#include "Video.h"

unsigned short* Diagnose::m_VideoMemory = (unsigned short *)(0xB8000 + 0xC0000000);
unsigned int Diagnose::m_Row = 10;
unsigned int Diagnose::m_Column = 0;

unsigned int Diagnose::ROWS = 10;

bool Diagnose::trace_on = true;

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
}

void Diagnose::WriteChar(const char ch)
{
	if(Diagnose::m_Column >= Diagnose::COLUMNS)
	{
		NextLine();
	}

	if(Diagnose::m_Row >= Diagnose::SCREEN_ROWS)
	{
		Diagnose::m_Row = Diagnose::SCREEN_ROWS - 1;
		Diagnose::ScrollScreen();
	}

	Diagnose::m_VideoMemory[Diagnose::m_Row * COLUMNS + Diagnose::m_Column] = (unsigned char) ch | Diagnose::COLOR;
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
	unsigned int startRow = Diagnose::SCREEN_ROWS - Diagnose::ROWS;

	/* ����һ�е����ݸ��Ƶ���һ�� */
	for ( i = 0; i < COLUMNS * (ROWS - 1); i++ )
	{
		Diagnose::m_VideoMemory[i + startRow * COLUMNS] = Diagnose::m_VideoMemory[i + startRow * COLUMNS + COLUMNS];
	}

	/* ��������һ�� */
	for ( i = COLUMNS * (ROWS - 1); i < COLUMNS * ROWS; i++ )
	{
		Diagnose::m_VideoMemory[i + startRow * COLUMNS] = (unsigned char)' ' | Diagnose::COLOR;
	}
}
