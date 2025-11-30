#ifndef MOUSE_H
#define MOUSE_H

#include "Regs.h"

class Mouse
{
	/* Const Member */
public:
	/* PS/2 鼠标I/O端口地址 */
	static const unsigned short DATA_PORT = 0x60;		/* 鼠标数据端口 */
	static const unsigned short STATUS_PORT = 0x64;		/* 鼠标状态端口 */
	static const unsigned short COMMAND_PORT = 0x64;	/* 鼠标命令端口 */

	/* 鼠标命令 */
	static const unsigned char MOUSE_WRITE = 0xD4;		/* 向鼠标写命令 */
	static const unsigned char MOUSE_ENABLE = 0xF4;		/* 启用鼠标数据报告 */
	static const unsigned char MOUSE_SET_SAMPLE = 0xF3;	/* 设置采样率 */
	static const unsigned char MOUSE_GET_ID = 0xF2;		/* 获取鼠标ID */

	/* 状态寄存器位定义 */
	static const unsigned char INPUT_BUFFER_FULL = 0x02;	/* 输入缓冲区满 */
	static const unsigned char OUTPUT_BUFFER_FULL = 0x01;	/* 输出缓冲区满 */

	/* 鼠标数据包标志位 */
	static const unsigned char LEFT_BUTTON = 0x01;		/* 左键按下 */
	static const unsigned char RIGHT_BUTTON = 0x02;		/* 右键按下 */
	static const unsigned char MIDDLE_BUTTON = 0x04;	/* 中键按下 */
	static const unsigned char X_SIGN = 0x10;			/* X位移符号位 */
	static const unsigned char Y_SIGN = 0x20;			/* Y位移符号位 */
	static const unsigned char X_OVERFLOW = 0x40;		/* X溢出 */
	static const unsigned char Y_OVERFLOW = 0x80;		/* Y溢出 */

	/* Functions */
public:
	/* 初始化鼠标驱动 */
	static void Initialize();

	/* 鼠标中断处理程序 */
	static void MouseHandler(struct pt_regs* reg, struct pt_context* context);

	/* 发送鼠标命令 */
	static void SendCommand(unsigned char command);

	/* 等待鼠标响应 */
	static unsigned char WaitResponse();

	/* 处理鼠标数据包 */
	static void HandleMousePacket();

	/* Members */
private:
	static unsigned char m_MousePacket[4];		/* 鼠标数据包缓冲区 */
	static unsigned int m_PacketIndex;			/* 当前数据包索引 */
	static unsigned char m_MouseID;				/* 鼠标ID (0=标准, 3=带滚轮) */
	static bool m_Initialized;					/* 是否已初始化 */
};

#endif
