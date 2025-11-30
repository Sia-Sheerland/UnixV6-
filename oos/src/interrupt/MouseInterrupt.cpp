#include "MouseInterrupt.h"
#include "Kernel.h"
#include "Regs.h"
#include "Mouse.h"
#include "IOPort.h"
#include "Chip8259A.h"

void MouseInterrupt::MouseInterruptEntrance()
{
	SaveContext();			/* 保存中断现场 */

	SwitchToKernel();		/* 切换到内核态 */

	CallHandler(Mouse, MouseHandler);		/* 调用鼠标中断设备驱动处理程序 */

	/* 向从8259A中断控制器芯片发送EOI命令 */
	IOPort::OutByte(Chip8259A::SLAVE_IO_PORT_1, Chip8259A::EOI);
	/* 向主8259A中断控制器芯片发送EOI命令 */
	IOPort::OutByte(Chip8259A::MASTER_IO_PORT_1, Chip8259A::EOI);

	/* 获取中断返回指令(由硬件实施)压入栈中的pt_context，
	 * 据此可以访问context.xcs中的OLD_CPL，中断前的态
	 * 是用户态，还是内核态。
	 */
	struct pt_context *context;
	__asm__ __volatile__ ("	movl %%ebp, %0; addl $0x4, %0 " : "+m" (context) );

	if( context->xcs & USER_MODE ) /*当前为用户态*/
	{
		while(true)
		{
			X86Assembly::CLI();	/* 关中断，优先级降为7。 */

			if(Kernel::Instance().GetProcessManager().RunRun > 0)
			{
				X86Assembly::STI();	/* 开中断，优先级降为0。 */
				Kernel::Instance().GetProcessManager().Swtch();
			}
			else
			{
				break;	/* 如果runrun == 0，出栈回到用户态，让用户进程继续执行 */
			}
		}
	}

	RestoreContext();		/* 恢复现场 */

	Leave();				/* 拆除当前栈帧 */

	InterruptReturn();		/* 退出中断 */
}
