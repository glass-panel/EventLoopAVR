# EventLoopAVR

该项目受 NodeJS 事件循环启发，旨在使用 C++17 在 AVR 8 单片机上实现一套简单的事件循环框架，如：

- [x] `eventloop.setTimeout()`
- [x] `eventloop.nextTick()`

### Requirements

1. `avr-g++` 版本11或更高以支持编译 C++17
2. 实现了 `std::tuple` 和 `std::apply` 的 C++ 标准库子集

该项目使用了 [modm-io 的 stdc++ 库](https://github.com/modm-io/avr-libstdcpp)，你也可以在同组织下找到预编译的新版 `avr-g++`

### Description

该项目由 `Task<>`、`CircularTaskQueue<>`、`EventLoop<>`、`Time` 类为核心，辅以 `PinT<>`、`Keys<>`、`PipeIO<>` 类提供对单片机IO的抽象

- `Task<>` 类实现了对 某一函数的 函数指针 及 函数参数 的打包并进行类型擦除，为事件循环对函数的延迟执行提供了基础
- `CircularTaskQueue<>` 类实现了栈上对 `Task<>` 对象的存储，避免了动态内存申请，并被设计为循环队列以配合事件循环的特性
- `EventLoop<>` 类实现了事件循环的主要功能，并使用 `CircularTaskQueue<>` 类存储事件循环中的任务
- `Time` 类实现了一个紧凑的(48bit)时间格式，并具有全局时间等的静态成员与对其的操作，为事件循环提供时间标准
- `PinT<>` 模板类使得在模板中对任意端口的某一个引脚的操作成为了可能
- `Keys<>` 模板类在 `PinT<>` 基础上更近一步，使得在编译期即可绑定引脚与按键，并生成对应的状态机函数，同时提供 `onClick` `onDoubleClick` 等的回调
- `PipeIO<>` 类对诸如 UART 等的外设提供了抽象，提供 `onData` 等的回调

该项目的平台依赖性不强，_兴许_ 可以移植至 STM32 等平台

### Usage

引入 EventLoopAVR.h 即可，但`std::tuple` 的实现可能需要实现 delete 运算符，尽管并没有调用

### Examples

1. 定义一个事件循环

   ```c++
   #include "EventLoopAVR.h"
   
   EventLoop<256, 24> eventloop;	// 声明一个256字节大小的队列用于存放任务，24个timeout任务槽位
   
   static const EventLoopHelperFunctions helper_functions
   {
       preQueueProcess,			// 每次任务队列执行前会执行的函数
       postQueueProcess,		// 每次任务队列执行后会执行的函数
       nullptr,					// 未实现
       nullptr,
   };
   // 在某个函数里：
   eventloop.setHelperFunctions(&helper_functions);	// 为事件循环设置以上函数
   // 某个一毫秒的定时器中断：
   ISR(TIMER_1MS_INTERRUPT, ISR_MODE)
   {
       Time::tick();								// 事件循环参考的时钟+1ms
   }
   ```

2. 使用 `nextTick` 推迟一个函数带参数的执行

   ```C++
   void someFunction(float a, int b)
   {
       ...;
   }
   // 在某个其他的函数里
   eventloop.nextTick(make_task(someFunction).setArgs({ 1.14, 514 }));	
   // 这个函数会与它的参数被推进事件循环的下一个队列内得到执行
   ```

3. 使用 `setTimeout` 将一个函数带参数推迟到n毫秒后执行(n<65535)

   ```C++
   void someFunction(float a, int b)
   {
       ...;
   }
   // 在某个其他的函数里
   eventloop.setTimeout(make_task(someFunction).setArgs({ 1.919, 810 }), n);
   // 这个函数会在n毫秒后执行，但并不保证一定是n毫秒
   ```

4. 使用 `Keys[n].onClick` 设置一个按下反转一次的引脚

   ```c++
   #include "Keys.h"
   
   static const sfr_wrapper PINB_W(PINB), PORTC_W(PORTC);	// 包装端口所对应的SFR
   Keys<PinT<PINB_W, 0>> keys;			// 将B端口0号引脚作为按键所在引脚
   
   void onThatKeyClick(uint8_t index)	// 处理按键的回调函数，index为按键号
   {
       PinT<PORTC_W, 1>::set( !PinT<PORTC_W, 1>::get() );	// 对C端口1号引脚反转
   }
   // 在某个函数里
   Keys[0].onClick = onThatKeyClick;
   ```

5. 使用 `PipeIO` 包装 UART

   ```c++
   #include "PipeIO.h"
   
   void uart_send_byte(char c)
   {
       while(!(UCSR0A & (1<<UDRE0)));  // wait for empty transmit buffer
       UDR0 = c;
   }
   
   char uart_buffer[128];				// 接收缓冲区
   PipeIO<uart_send_byte> uart(uart_buffer, sizeof(uart_buffer));
   
   void onUartData(PipeIO<uart_send_byte>* self, char* prev)
   {
        // do sth with self->buffer()
        // clear ondata flag
        self->flags() = self->flags() & (~(uint8_t)PipeIOFlags::ONDATA);
   }
   
   // 在某个函数里
   uart << "hello world!" << 114514;
   // UART的接收完成中断
   ISR(USART_RX_vect, ISR_MODE)
   {
       char c = UDR0;
       uart.buffer_push(c);			// 将收到的字符推入缓冲区内
   }
   // 在某个函数内
   uart.onData = onUartData;	    // 设置接收回调函数
   ```

### Notice

1. 如需要在中断中调用 `nextTick` 或 `setTimeout`，请**务必务必**注意这些函数的重入冲突问题；目前作者建议使用事件循环前会调用的 `preQueueProcess` 函数根据事件所设置的 flag 再推入任务
2. 出于以上考虑及对各组件的隔离与抽象，`Keys`、`PipeIO` 等对象提供 `.updateState()` 或 `.checkEvents()` 的成员函数完成对事件所设置 flag 的检查与更新，并在其中**直接**调用用户定义的回调函数，可以选择在中断触发时立刻调用这些成员函数以立即执行回调，也可以在事件循环中轮询；也就是说，这些回调函数不会被压入事件循环中，如需要实现完全类似 Javascript 的逻辑，还需手动编写将事件处理函数加入事件循环队列的回调
3. 事件循环在队列中没有任何任务与timeout队列也为空时会退出，可在 `postQueueProcess` 中加入队列空判断推入一个不做任何事的任务保活
4. **这项目还没完全写完** ╮(╯▽╰)╭

### TODOs

高优先:

- [ ] `onTaskAllocationFailed`、`onTimeoutNodeAllocationFailed`等
- [ ] lambda函数支持

低优先：

- [ ] 自行提供 `std::tuple` 与 `std::apply` 的实现以摆脱对 `avr-stdc++` 库依赖





