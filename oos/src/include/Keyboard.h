#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "Regs.h"

class Keyboard
{
	/* Const Member */
public:
	/* ����I/O�˿ڵ�ַ */
	static const unsigned short DATA_PORT = 0x60;	/* �������ݼĴ����˿ں� */
	static const unsigned short STATUS_PORT = 0x64;	/* ����״̬�Ĵ����˿ں� */

	/* ״̬�Ĵ�������λ���� (�˿ںţ�0x64) */
	static const unsigned char DATA_BUFFER_BUSY = 0x1;	/* ������������Ƿ��� */

	/* ɨ���볣������ */
	static const unsigned char SCAN_ALT = 0x38;
	static const unsigned char SCAN_CTRL = 0x1d;
	static const unsigned char SCAN_LSHIFT = 0x2a;
	static const unsigned char SCAN_RSHIFT = 0X36;
	static const unsigned char SCAN_ESC = 0x01;
	static const unsigned char SCAN_NUMLOCK = 0x45;
	static const unsigned char SCAN_CAPSLOCK = 0x3a;
	static const unsigned char SCAN_SCRLOCK = 0x46;

	/* �����ɨ���� (��Ҫ 0xE0 ǰ׺) */
	static const unsigned char SCAN_UP = 0x48;
	static const unsigned char SCAN_DOWN = 0x50;
	static const unsigned char SCAN_PAGEUP = 0x49;
	static const unsigned char SCAN_PAGEDOWN = 0x51;

	/* ���ڽ��㿪��״̬ */
	enum ScrollFocus
	{
		FOCUS_CRT = 0,		/* �����ڴ��ڣ��Ͻ��� */
		FOCUS_DIAGNOSE = 1	/* ��־���ڣ��½��� */
	};

	/* ����Ϊ���Ƽ����µ�״̬��mode�и�����λ�Ķ��� */
	static const int M_LCTRL = 0x01;
	static const int M_RCTRL = 0x02;
	static const int M_LALT = 0x04;
	static const int M_RALT = 0x08;
	static const int M_LSHIFT = 0x10;
	static const int M_RSHIFT = 0x20;
	static const int M_NUMLOCK = 0x40;
	static const int M_CAPSLOCK = 0x80;
	static const int M_SCRLOCK = 0x100;
	static const int M_DOWN_NUMLOCK = 0x200;
	static const int M_DOWN_CAPSLOCK = 0x400;
	static const int M_DOWN_SCRLOCK = 0x800;

	/* Functions */
public:
	/* �����ж��豸�����ӳ��� */
	static void KeyboardHandler(struct pt_regs* reg, struct pt_context* context);

	/* 
	 * ����ɨ�����ӳ��� scanCode(ɨ����) expand(��չ��)��
	 * expand��ʾ�Ƿ�����չ�ļ�����Ҫ�����ж����ҵ�ctrl��alt
	 */
	static void HandleScanCode(unsigned char scanCode, int expand);

	/* ������ɨ����ת��Ϊ��Ӧ��ASCII�� */
	static char ScanCodeTranslate(unsigned char scanCode, int expand);

	/* �л����ڽ��� */
	static void ToggleFocus();

	/* ��ȡ��ǰ���ڽ��� */
	static ScrollFocus GetFocus() { return m_ScrollFocus; }

	/* Members */
public:
	/* 
	 * ����ӳ�����ȡ��һ�����̼����Ӽ�������<fx>���ܼ�û
	 * ����ơ����������0��asc���ʾ�ڸ�ϵͳ��û��ӳ�䡣
	 * keymap ����shift��û�а��µ������ɨ�����ӳ�����
	 * �������ڴ���0x45��ɨ���룬Ҳ����С�������빦������ɨ
	 * ���룬�����ʾnumlock�����µ����
	 */
	static char Keymap[];

	/* shift_keymap����shift���µ���� */
	static char Shift_Keymap[];

	/* ctrl, alt, shift��״̬������numlock,capslock,scrlock�����ɿ�*/
	static int Mode;

	/* ��ǰ���ڽ���״̬ */
	static ScrollFocus m_ScrollFocus;
};

#endif
