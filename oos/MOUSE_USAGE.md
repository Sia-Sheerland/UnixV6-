# 鼠标滚轮支持使用说明

## 功能概述

为 Unix V6++ 系统添加了完整的 PS/2 鼠标驱动，支持鼠标滚轮操作来滚动屏幕历史内容。

## 文件清单

- `oos/src/include/Mouse.h` - 鼠标驱动头文件
- `oos/src/tty/Mouse.cpp` - 鼠标驱动实现

## 功能特性

### ✅ 已实现
- **PS/2 鼠标初始化**：自动检测鼠标设备
- **滚轮支持**：自动检测是否支持滚轮（鼠标ID=3）
- **屏幕滚动**：
  - 向上滚轮：查看历史内容（每次1行）
  - 向下滚轮：返回最新内容（每次1行）
- **智能模式**：与键盘滚动共享自动/手动模式

### 🔧 技术细节

#### 鼠标滚轮激活序列
```cpp
设置采样率 200 →
设置采样率 100 →
设置采样率 80  →
获取鼠标ID     → ID=3 表示支持滚轮
```

#### 数据包格式
```
标准鼠标（3字节）:
Byte 0: [Y溢出|X溢出|Y符号|X符号|1|中键|右键|左键]
Byte 1: X位移 (8位有符号)
Byte 2: Y位移 (8位有符号)

带滚轮鼠标（4字节）:
Byte 0-2: 同上
Byte 3: [按键4|按键5|0|0|滚轮数据(4位有符号)]
```

#### 滚轮处理逻辑
```cpp
signed char wheelDelta = m_MousePacket[3] & 0x0F;

if (wheelDelta & 0x08)  // 符号扩展
    wheelDelta |= 0xF0;

if (wheelDelta > 0)      // 向上滚动
    CRT::ScrollUp(1);
else if (wheelDelta < 0) // 向下滚动
    CRT::ScrollDown(1);
```

## 集成步骤

### 1. 初始化鼠标驱动

在系统启动时调用：
```cpp
#include "Mouse.h"

// 在内核初始化代码中
Mouse::Initialize();
```

### 2. 注册鼠标中断

在中断向量表中注册 IRQ12（鼠标中断）：
```cpp
// 示例：在中断初始化代码中
SetInterruptHandler(IRQ12_VECTOR, Mouse::MouseHandler);
```

**IRQ12 向量号计算**：
```
IRQ12 = PIC_OFFSET + 12
通常：0x20 + 12 = 0x2C (44)
```

### 3. 启用从PIC的IRQ12

```cpp
// 启用从8259A PIC的IRQ12
unsigned char mask = IOPort::InByte(0xA1);  // 读从PIC屏蔽寄存器
mask &= ~0x10;  // 清除bit 4 (IRQ12)
IOPort::OutByte(0xA1, mask);  // 写回
```

## 使用方式

### 鼠标滚轮操作

| 操作 | 功能 | 滚动量 |
|------|------|--------|
| **滚轮向上滚** 🖱️↑ | 查看历史内容 | 1行/次 |
| **滚轮向下滚** 🖱️↓ | 返回最新内容 | 1行/次 |

### 与键盘滚动配合

| 输入方式 | Page Up/Down | 方向键↑↓ | 鼠标滚轮 |
|---------|-------------|---------|---------|
| **速度** | 快（5行） | 中（1行） | 慢（1行） |
| **精确度** | 低 | 高 | 高 |
| **推荐用途** | 快速翻页 | 精确定位 | 浏览历史 |

## 调试信息

初始化成功后会输出：
```
Mouse: Initialized with scroll wheel support (ID=3)
```

或者（如果不支持滚轮）：
```
Mouse: Initialized without scroll wheel (ID=0)
```

## 兼容性

### 支持的鼠标类型
- ✅ 标准PS/2鼠标（3按键）
- ✅ 带滚轮PS/2鼠标（IntelliMouse）
- ✅ 带5按键的鼠标（部分功能）

### 虚拟机支持
- ✅ VMware
- ✅ VirtualBox
- ✅ QEMU
- ✅ Bochs

### 物理机支持
- ✅ 带PS/2接口的台式机
- ⚠️ 现代笔记本（可能需要USB转PS/2）
- ❌ 纯USB鼠标（需要USB驱动）

## 常见问题

### Q: 鼠标滚轮没反应？
**A**: 检查以下项：
1. 确认已调用 `Mouse::Initialize()`
2. 确认已注册IRQ12中断
3. 确认已启用从PIC的IRQ12
4. 查看调试输出确认鼠标ID

### Q: 如何调整滚动速度？
**A**: 修改 `Mouse.cpp` 中的滚动行数：
```cpp
// 当前每次滚动1行
CRT::ScrollUp(1);   // 改为 3 可以每次滚动3行
CRT::ScrollDown(1); // 改为 3 可以每次滚动3行
```

### Q: 支持Diagnose窗口滚动吗？
**A**: 当前只支持CRT窗口。要支持Diagnose，修改：
```cpp
// 在 Mouse.cpp 的 HandleMousePacket() 中添加
if (wheelDelta > 0)
{
    CRT::ScrollUp(1);
    Diagnose::ScrollUp(1);  // 同时滚动调试窗口
}
```

## 未来扩展

### 计划中的功能
- [ ] 鼠标移动光标
- [ ] 鼠标点击选择文本
- [ ] 复制粘贴支持
- [ ] 可配置滚动速度
- [ ] 侧键（按键4/5）支持

## 技术参考

### PS/2 鼠标协议
- 端口：0x60 (数据), 0x64 (命令/状态)
- 中断：IRQ12 (从PIC)
- 时钟频率：10-16.7 kHz
- 数据速率：10-40 字节/秒

### 相关资源
- [OSDev Wiki - PS/2 Mouse](https://wiki.osdev.org/PS/2_Mouse)
- [Intel IntelliMouse 协议](https://isdaman.com/alsos/hardware/mouse/ps2interface.htm)

---

**版本**：v1.0
**创建日期**：2025年11月30日
**作者**：Claude (Anthropic AI)
