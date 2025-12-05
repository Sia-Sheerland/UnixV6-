#include "Keyboard.h"
#include "IOPort.h"
#include "Kernel.h"
#include "ProcessManager.h"
#include "Video.h"
#include "CharDevice.h"
#include "CRT.h"

char Keyboard::Keymap[] = {
	0,0x1b,'1','2','3','4','5','6',		/* 0x00-0x07 0, <esc>,1,2,3,4,5,6, */
	'7','8','9','0','-','=',0x8,0x9,	/* 0x08-0x0f 7,8,9,0,-,=,<backspace><tab>*/
	'q','w','e','r','t','y','u','i',	/* 0x10-0x17 qwertyui*/
	'o','p','[',']','\n',0,'a','s', 	/* 0x18-0x1f op[] <enter><ctrl>as */
	'd','f','g','h','j','k','l',';',	/* 0x20-0x27 dfghjkl; */
	'\'','`',0,'\\','z','x','c','v',	/* 0x28-0x2f '`<lshift>\zdcv */
	'b','n','m',',','.','/',0,'*', 		/* 0x30-0x37 bnm,./<rshitf><printscr> */
	0,' ',0,0,0,0,0,0, 					/* 0x38-0x3f <alt><space><caps><f1><f2><f3><f4><f5> */
	0,0,0,0,0,0,0,'7', 					/* 0x40-0x47 <f0><><><><><numlock><scrlock>7*/
	'8','9','-','4','5','6','+','1',	/* 0x48-x04f 89-456+1  */
	'2','3','0','.',0,0,0,0				/* 0x50-0x57 230.<><><><>  */
};

char Keyboard::Shift_Keymap[] = {
	0,0x1b,'!','@','#','$','%','^',		/* 0x00-0x07 0, <esc>,!,@,#,$,%,^, */
	'&','*','(',')','_','+',0x8,0x9,	/* 0x08-0x0f ~,<backspace><tab>*/
	'q','w','e','r','t','y','u','i',	/* 0x10-0x17 qwertyui*/
	'o','p','{','}','\n',0,'a','s', 	/* 0x18-0x1f op[] <enter><ctrl>as */
	'd','f','g','h','j','k','l',':',	/* 0x20-0x27 dfghjkl; */
	'\"','~',0,'|','z','x','c','v',		/* 0x28-0x2f '`<lshift>\zdcv */
	'b','n','m','<','>','?',0,'*', 		/* 0x30-0x37 bnm,./<rshitf><printscr> */
	0,' ',0,0,0,0,0,0, 					/* 0x38-0x3f <alt><space><caps><f1><f2><f3><f4><f5> */
	0,0,0,0,0,0,0,0, 					/* 0x40-0x47 <f0><><><><><numlock><scrlock>7*/
	0,0,'-',0,0,0,'+',0,				/* 0x48-x04f 89-456+1  */
	0,0,0,0x7f/*del*/,0,0,0,0			/* 0x50-0x57 230.<><><><>  */
};

int Keyboard::Mode = 0;

/* ��ʼ����ǰ���ڽ���Ϊ���봰�� */
Keyboard::ScrollFocus Keyboard::m_ScrollFocus = Keyboard::FOCUS_CRT;

void Keyboard::KeyboardHandler( struct pt_regs* reg, struct pt_context* context )
{
	/***********************************************/
	//for test
	//Diagnose::Write("key pressed !   ");
	/***********************************************/

	unsigned char status = IOPort::InByte(Keyboard::STATUS_PORT);
	int limit = 10;
	static int pre_state = 0;

	while ( (status & Keyboard::DATA_BUFFER_BUSY) && limit-- )
	{
		/* ������̻�������Ҫ����ʣ�µ�ɨ���� */
		unsigned char scancode = IOPort::InByte(Keyboard::DATA_PORT);

		/* ���µ��жϹ����е�������״̬��������ò���еĵط�û�л��� */
		if ( 0 == pre_state )
		{
			if ( 0xE0 == scancode || 0xE1 == scancode )
			{
				/* ��״̬��Ϊ0xe0��ʾ�����ַ�û�ж��� */
				/* ����0xe1ֻ��һ��״̬���Ǿ���pause�������£�������������Ϊ0xe1,0x1d,0x45�������Ͽ�����Ϊ0xe1,0x9d,0xc5  */
				pre_state = scancode;
			}
			else	/* ����չ�� */
			{
				pre_state = 0;
				Keyboard::HandleScanCode(scancode, 0);
			}
		}
		else if ( 0xE0 == pre_state )
		{
			/* ��չ���ĵڶ���ɨ���� */
			pre_state = 0;
			Keyboard::HandleScanCode(scancode, 0xe0);
		}
		else if ( 0xE1 == pre_state && ( 0x1d == scancode || 0x9d == scancode ) )
		{
			pre_state = 0x100;	/* �м�״̬����ʾpause�Ѿ����������Ǻ� */
		}
		else if ( pre_state == 0x100 && 0x45 == scancode )	/* ֻ��Ҫ֪��pause��ʲôʱ���� */
		{
			pre_state = 0;
			Keyboard::HandleScanCode(scancode, 0xe1);
		}
		else
		{
			pre_state = 0;
		}
		status = IOPort::InByte(Keyboard::STATUS_PORT);
	}
}

void Keyboard::HandleScanCode(unsigned char scanCode, int expand)
{
	int isOK = 0;
	char ch = 0;

	if ( 0xE1 == expand )
	{
		ch = Keyboard::ScanCodeTranslate(scanCode, expand);
	}

	switch ( scanCode )
	{
	case SCAN_ALT:
		if ( 0xE0 == expand )	/* ʹ����չ������ʾ���ұߵ� alt���� */
			Mode |= M_RALT;
		else
			Mode |= M_LALT;
		break;

	case SCAN_CTRL:
		if ( 0xE0 == expand )	/* ʹ����չ������ʾ���ұߵ� ctrl���� */
			Mode |= M_RCTRL;
		else
			Mode |= M_LCTRL;
		break;

	case SCAN_LSHIFT:
		Mode |= M_LSHIFT;
		break;

	case SCAN_RSHIFT:
		Mode |= M_RSHIFT;
		break;

	/* �����������ɿ�����������Mode�а�����Ӧ�ı�־λ */
	case SCAN_ALT + 0x80:
		if ( 0xE0 == expand )
			Mode &= ~M_RALT;
		else
			Mode &= ~M_LALT;
		break;

	case SCAN_CTRL + 0x80:
		if ( 0xE0 == expand )
			Mode &= ~M_RCTRL;
		else
			Mode &= ~M_LCTRL;
		break;

	case SCAN_LSHIFT + 0x80:
		Mode &= ~M_LSHIFT;
		break;

	case SCAN_RSHIFT + 0x80:
		Mode &= ~M_RSHIFT;
		break;

	/* ÿ�ΰ��¾ͷ�ת�ı�״̬λ */
	
	/* 
	 * M_DOWN_NUMLOCK���ʾNUMLOCK�����£���û�а��µ�
	 * ״̬�°���NumLock����Ҫ��ת״̬������numlock��û���ɿ�
	 */
	case SCAN_NUMLOCK:
		isOK = Mode & M_DOWN_NUMLOCK;
		if ( !isOK )
		{
			Mode ^= M_NUMLOCK;
			Mode |= M_DOWN_NUMLOCK;
		}
		break;

	case SCAN_CAPSLOCK:
		isOK = Mode & M_DOWN_CAPSLOCK;
		if ( !isOK )
		{
			Mode ^= M_CAPSLOCK;
			Mode |= M_DOWN_CAPSLOCK;
		}
		break;

	case SCAN_SCRLOCK:
		isOK = Mode & M_DOWN_SCRLOCK;
		if ( !isOK )
		{
			Mode ^= M_SCRLOCK;
			Mode |= M_DOWN_SCRLOCK;
		}
		break;

	/* �ͷ�NumLock��CapsLock��ScrollLock�� */
	case SCAN_NUMLOCK + 0x80:
		Mode &= ~M_DOWN_NUMLOCK;
		break;

	case SCAN_CAPSLOCK + 0x80:
		Mode &= ~M_DOWN_CAPSLOCK;
		break;

	case SCAN_SCRLOCK + 0x80:
		Mode &= ~M_DOWN_SCRLOCK;
		break;

	/* Tab �� - �л����ڽ��� */
	case 0x0F:	/* Tab ɨ���� */
		if ( !(scanCode & 0x80) )  /* ֻ���¼����ɿ� */
		{
			ToggleFocus();
		}
		break;

	/* ��������ɨ��������Ļ���� - ���ݽ����·��䵽��Ӧ���� */
	case SCAN_PAGEUP:
		if ( 0xE0 == expand )	/* Page Up �� ��չ�� */
		{
			if ( m_ScrollFocus == FOCUS_CRT )
				CRT::ScrollUp(5);	/* �����ڣ����϶�5�� */
			else
				Diagnose::ScrollUp(5);	/* ��־���ڣ����϶�5�� */
		}
		break;

	case SCAN_PAGEDOWN:
		if ( 0xE0 == expand )	/* Page Down �� ��չ�� */
		{
			if ( m_ScrollFocus == FOCUS_CRT )
				CRT::ScrollDown(5);	/* �����ڣ����¹���5�� */
			else
				Diagnose::ScrollDown(5);	/* ��־���ڣ����¹���5�� */
		}
		break;

	case SCAN_UP:
		if ( 0xE0 == expand )	/* �����ͷ�� ��չ�� */
		{
			if ( m_ScrollFocus == FOCUS_CRT )
				CRT::ScrollUp(1);	/* �����ڣ����϶�1�� */
			else
				Diagnose::ScrollUp(1);	/* ��־���ڣ����϶�1�� */
		}
		break;

	case SCAN_DOWN:
		if ( 0xE0 == expand )	/* �����·�� ��չ�� */
		{
			if ( m_ScrollFocus == FOCUS_CRT )
				CRT::ScrollDown(1);	/* �����ڣ����¹���1�� */
			else
				Diagnose::ScrollDown(1);	/* ��־���ڣ����¹���1�� */
		}
		break;

	default:
		ch = Keyboard::ScanCodeTranslate(scanCode, expand);
		break;
	}
	if ( 0 != ch )
	{
		TTy* pTTy = Kernel::Instance().GetDeviceManager().GetCharDevice(DeviceManager::TTYDEV).m_TTy;
		if ( NULL != pTTy )
		{
			pTTy->TTyInput(ch);
		}
	}
}

char Keyboard::
ScanCodeTranslate(unsigned char scanCode, int expand)
{
	char ch = 0;
	bool bReverse = false;

	if ( 0xE1 == expand )	/* Pause Key */
	{
		ch = 0x05;	/* Pause ASCII */
	}
	else if ( scanCode < 0x45 )	/* ��С���̺Ϳ��Ƽ� */
	{
		/* ����ɨ����ӳ����ҵ���Ӧ������ASCII�� */
		if ( (Mode & M_LSHIFT) || (Mode & M_RSHIFT) )
		{
			ch = Shift_Keymap[scanCode];
		}
		else
		{
			ch = Keymap[scanCode];
		}

		if ( ch >= 'a' && ch <= 'z' )
		{
			/* ��Сд�ַ������Ѿ�Capslock�ˣ���ôת���ɴ�д�ַ� */
			bReverse = ( (Mode & M_CAPSLOCK) ? 1 : 0 ) ^ ( (Mode & M_LSHIFT) || (Mode & M_RSHIFT) );

			if ( (Mode & M_LCTRL) || (Mode & M_RCTRL) )	/* ����ctrl����ת�� */
			{
				if('c' ==  ch)  /* ctrl+c --> SIGINT�źţ��͸��� sched��shell ֮������н��� */
				{
					ch = 0;

					/* FLush�ն� */
					TTy* pTTy = Kernel::Instance().GetDeviceManager().GetCharDevice(DeviceManager::TTYDEV).m_TTy;
					if ( NULL != pTTy )
					{
						pTTy->FlushTTy();
					}

					ProcessManager& procMgr = Kernel::Instance().GetProcessManager();
					for ( int killed = 0; killed < ProcessManager::NPROC ; killed ++ )
						if ( procMgr.process[killed].p_pid > 1)
							procMgr.process[killed].PSignal(User::SIGINT);
				}
				else
				{
					ch -= 'a';
				    ch++;	/* ת���0x1 ��ʼ*/
				}
			}
			else if ( bReverse )
			{
				ch += 'A' - 'a';
			}
		}
	}
	else if ( scanCode < 0x58 )
	{
		bReverse = ( (Mode & M_NUMLOCK) ? 1 : 0 ) ^ ( (Mode & M_LSHIFT) || (Mode & M_RSHIFT) );

		if ( 0xE0 == expand )
		{
			ch = Shift_Keymap[scanCode];
		}
		else if ( bReverse )
		{
			ch = Keymap[scanCode];
		}
		else
		{
			ch = Shift_Keymap[scanCode];
		}
	}

	return ch;
}

void Keyboard::ToggleFocus()
{
	/* �л����ڽ��� */
	if ( m_ScrollFocus == FOCUS_CRT )
	{
		m_ScrollFocus = FOCUS_DIAGNOSE;
		Diagnose::Write("[Focus: Diagnose Window]\n");
	}
	else
	{
		m_ScrollFocus = FOCUS_CRT;
		Diagnose::Write("[Focus: CRT Window]\n");
	}
}

