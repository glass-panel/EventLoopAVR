# EventLoopAVR

该项目受 NodeJS 事件循环启发，旨在使用 C++14或以上版本 在 AVR 8 单片机上实现一套简单的事件循环框架，如：

- [x] `eventloop.setTimeout()`
- [x] `eventloop.nextTick()`

### Requirements

1. `avr-g++` 至少支持编译 C++14 的版本

该项目使用了部分 stdc++ 库中的元编程模板，为此本项目自带了对所需模板非常粗糙的实现 (no_stdcpp_lib.h) 

如需完整的 stdc++ 库，可以考虑该项目曾采用的 [modm-io 的 stdc++ 库](https://github.com/modm-io/avr-libstdcpp)，你也可以在同组织下找到预编译的高版本 `avr-g++`

### Features

1. 零动态内存分配
2. 支持将自定义参数传入延迟执行的任务，而不需借助动态内存分配，如 `eventloop.nextTick(make_task(some_function).setArgs(some_args_tuple))`
3. 支持设置超时任务，并可使用所计划的函数指针取消该超时任务
4. (部分)简化IO设置，可在编译期确定引脚与按键的绑定、为串口提供流输出操作符等

### Description

该项目由 `Task<>`、`CircularTaskQueue<>`、`EventLoop<>`、`Time` 类为核心，实现了事件循环的框架，辅以 `PinT<>`、`Keys<>`、`PipeIO<>` 类提供对单片机IO的抽象

- `Task<>` 类实现了对 某一函数的 函数指针 及 函数参数 的打包并进行类型擦除，为事件循环对函数的延迟执行提供了基础
- `CircularTaskQueue<>` 类实现了栈上对 `Task<>` 对象的存储，避免了动态内存申请，并被设计为循环队列以配合事件循环的特性
- `EventLoop<>` 类实现了事件循环的主要功能，并使用 `CircularTaskQueue<>` 类存储事件循环中的任务
- `Time` 类实现了一个紧凑的(48bit)时间格式，并具有全局时间等的静态成员与对其的操作，为事件循环提供时间标准
- `PinT<>` 模板类使得在模板中对任意端口的某一个引脚的操作成为了可能(C++17 only)
- `Keys<>` 模板类在 `PinT<>` 基础上更近一步，使得在编译期即可绑定引脚与按键，并生成对应的状态机函数，同时提供 `onClick` `onDoubleClick` 等的回调(C++17 only)
- `PipeIO<>` 类对诸如 UART 等的外设提供了抽象，提供 `onData` 等的回调

该项目的平台依赖性不强，_兴许_ 可以移植至 STM32 等平台

### Usage

引入 EventLoopAVR.h 即可，但请注意对部分 stl 模板的自己实现会污染 `std` 命名空间

### Examples

参考 examples/ 文件夹

### Notice

1. 如需要在中断中调用 `nextTick` 或 `setTimeout`，请**务必务必**注意这些函数的重入冲突问题；目前建议使用事件循环前会调用的 `preQueueProcess` 函数根据事件所设置的 flag 再推入任务
2. 出于以上考虑及对各组件的隔离与抽象，`Keys`、`PipeIO` 等对象提供 `.updateState()` 或 `.checkEvents()` 的成员函数完成对事件所设置 flag 的检查与更新，并在其中**直接**调用用户定义的回调函数，可以选择在中断触发时立刻调用这些成员函数以立即执行回调，也可以在事件循环中轮询；也就是说，这些回调函数不会被压入事件循环中，如需要实现完全类似 Javascript 的逻辑，还需手动编写将事件处理函数加入事件循环队列的回调
3. 事件循环在队列中没有任何任务与timeout队列也为空时会退出，可在 `postQueueProcess` 中加入队列空判断推入一个不做任何事的任务保活
4. PinT<> 模板需要 C++17 才能使用，尽管语法和所需特性跟 C++17 没什么关系，我也不知道为啥他不能工作在低版本
5. **这项目还没完全写完** ╮(╯▽╰)╭

### TODOs

- [ ] `onTaskAllocationFailed`、`onTimeoutNodeAllocationFailed`等
- [ ] Arduino IDE Support
