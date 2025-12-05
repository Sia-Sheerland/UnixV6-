#include "Mouse.h"
#include "IOPort.h"
#include "CRT.h"
#include "Video.h"
#include "Keyboard.h"

/* 静态成员变量初始化 */
unsigned char Mouse::m_MousePacket[4] = {0, 0, 0, 0};
unsigned int Mouse::m_PacketIndex = 0;
unsigned char Mouse::m_MouseID = 0;
bool Mouse::m_Initialized = false;

void Mouse::Initialize()
{
	if (m_Initialized)
		return;

	/* 等待输入缓冲区空 */
	while (IOPort::InByte(STATUS_PORT) & INPUT_BUFFER_FULL);

	/* 启用辅助设备（鼠标）*/
	IOPort::OutByte(COMMAND_PORT, 0xA8);

	/* 等待输入缓冲区空 */
	while (IOPort::InByte(STATUS_PORT) & INPUT_BUFFER_FULL);

	/* 获取控制器配置字节 */
	IOPort::OutByte(COMMAND_PORT, 0x20);
	unsigned char status = WaitResponse();

	/* 启用鼠标中断(IRQ12) */
	status |= 0x02;
	status &= ~0x20;  /* 清除鼠标时钟禁用位 */

	/* 写回配置字节 */
	while (IOPort::InByte(STATUS_PORT) & INPUT_BUFFER_FULL);
	IOPort::OutByte(COMMAND_PORT, 0x60);
	while (IOPort::InByte(STATUS_PORT) & INPUT_BUFFER_FULL);
	IOPort::OutByte(DATA_PORT, status);

	/* 尝试启用滚轮支持 */
	/* 通过设置特定的采样率序列来激活滚轮模式 */
	SendCommand(MOUSE_SET_SAMPLE);
	WaitResponse();
	SendCommand(200);  /* 设置采样率为200 */
	WaitResponse();

	SendCommand(MOUSE_SET_SAMPLE);
	WaitResponse();
	SendCommand(100);  /* 设置采样率为100 */
	WaitResponse();

	SendCommand(MOUSE_SET_SAMPLE);
	WaitResponse();
	SendCommand(80);   /* 设置采样率为80 */
	WaitResponse();

	/* 获取鼠标ID，检查是否支持滚轮 */
	SendCommand(MOUSE_GET_ID);
	WaitResponse();
	m_MouseID = WaitResponse();

	/* 启用鼠标数据报告 */
	SendCommand(MOUSE_ENABLE);
	WaitResponse();

	m_Initialized = true;

	/* 输出调试信息 */
	if (m_MouseID == 3)
	{
		Diagnose::Write("Mouse: Initialized with scroll wheel support (ID=%d)\n", m_MouseID);
	}
	else
	{
		Diagnose::Write("Mouse: Initialized without scroll wheel (ID=%d)\n", m_MouseID);
	}
}

void Mouse::SendCommand(unsigned char command)
{
	/* 等待输入缓冲区空 */
	while (IOPort::InByte(STATUS_PORT) & INPUT_BUFFER_FULL);

	/* 发送"写入鼠标"命令 */
	IOPort::OutByte(COMMAND_PORT, MOUSE_WRITE);

	/* 等待输入缓冲区空 */
	while (IOPort::InByte(STATUS_PORT) & INPUT_BUFFER_FULL);

	/* 发送鼠标命令 */
	IOPort::OutByte(DATA_PORT, command);
}

unsigned char Mouse::WaitResponse()
{
	/* 等待输出缓冲区满 */
	while (!(IOPort::InByte(STATUS_PORT) & OUTPUT_BUFFER_FULL));

	/* 读取响应 */
	return IOPort::InByte(DATA_PORT);
}

void Mouse::MouseHandler(struct pt_regs* reg, struct pt_context* context)
{
	/* 检查是否有数据可读 */
	if (!(IOPort::InByte(STATUS_PORT) & OUTPUT_BUFFER_FULL))
		return;

	/* 读取鼠标数据 */
	unsigned char data = IOPort::InByte(DATA_PORT);

	/* 存储到数据包缓冲区 */
	m_MousePacket[m_PacketIndex] = data;
	m_PacketIndex++;

	/* 根据鼠标ID确定数据包大小 */
	unsigned int packetSize = (m_MouseID == 3) ? 4 : 3;

	/* 如果收集到完整的数据包，处理它 */
	if (m_PacketIndex >= packetSize)
	{
		HandleMousePacket();
		m_PacketIndex = 0;
	}
}

void Mouse::HandleMousePacket()
{
	unsigned char flags = m_MousePacket[0];

	/* 检查数据包有效性 (bit 3 必须为1) */
	if (!(flags & 0x08))
	{
		m_PacketIndex = 0;
		return;
	}

	/* 解析鼠标移动和按键状态 */
	// char deltaX = m_MousePacket[1];
	// char deltaY = m_MousePacket[2];
	// bool leftButton = flags & LEFT_BUTTON;
	// bool rightButton = flags & RIGHT_BUTTON;
	// bool middleButton = flags & MIDDLE_BUTTON;

	/* 处理滚轮数据（如果支持）*/
	if (m_MouseID == 3 && m_PacketIndex >= 4)
	{
		/* 第4字节是滚轮数据 (有符号字节) */
		signed char wheelDelta = (signed char)m_MousePacket[3];

		/* 只处理低4位，忽略高4位的按键信息 */
		wheelDelta = (signed char)((wheelDelta & 0x0F));

		/* 如果是负数，进行符号扩展 */
		if (wheelDelta & 0x08)
		{
			wheelDelta |= 0xF0;
		}

		/* ���ݽ��㴰���л����� */
		if (wheelDelta > 0)
		{
			/* 向上滚动滚轮 - 向下滚动屏幕内容（查看新内容）*/
			if ( Keyboard::GetFocus() == Keyboard::FOCUS_CRT )
				CRT::ScrollDown(1);
			else
				Diagnose::ScrollDown(1);
		}
		else if (wheelDelta < 0)
		{
			/* 向下滚动滚轮 - 向上滚动屏幕内容（查看历史）*/
			if ( Keyboard::GetFocus() == Keyboard::FOCUS_CRT )
				CRT::ScrollUp(1);
			else
				Diagnose::ScrollUp(1);
		}
	}

	/* 未来可以在这里添加鼠标移动和点击的处理 */
}
