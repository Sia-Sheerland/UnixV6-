#ifndef MOUSE_INTERRUPT_H
#define MOUSE_INTERRUPT_H

class MouseInterrupt
{
public:
	/* 鼠标中断内核处理入口函数，在IDT的鼠标中断对应中断门 */
	static void MouseInterruptEntrance();
};

#endif
