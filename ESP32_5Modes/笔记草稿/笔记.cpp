// clang-format off
// @formatter:off
// NOLINTBEGIN

#define _TIPS
#ifndef _TIPS

不论嵌套多少层if，
if内的break用来退出最近一个while，
return用来退出最近一个func
while内的return能退出while外的func，
func内的break不能退出func外的while /*不能调用栈，编译报错*/
continue 用来在 while 中跳过后面直接进入下一个循环

嵌套多层if时，没有 #控制流语句# 用于退出上一层 if
层层嵌套时，为避免用成对的if(){标志}+if(标志){}，
可以用一个 #提取function# 调用 return 退出
但是用 #do{}while(0)# + break 方便多了
多条件判断时为了避免层层嵌套，用 goto 跳转

for 是 while 的简写 //但是有个坑！for 的 continue 会触发 update (i++)，while 的 continue 直接跳判断。
//所以while里i++放continue前面时两者一样？No,如果比如用i作为数组指针，你得先用了i再 i++。
//此时for更安全，等于自动放一个i++在continue前。除非你不嫌麻烦，while(?)...{if (skip){i++; continue;}...}
for (; goCanada ;) {   } //这等价于 while(goCanada)
for (;;) {   } //等价于 while(1)
for(A; B; C) 的严谨执行顺序：执行 A (仅一次)。判断 B (若为假，退出)。执行 循环体。 // for其实超灵活还更安全

do{}while()会至少执行一遍，也相当于 while 的简写
else if 是 else{if(){}}的简写
if(val = 123) ，此处返回值为 123 而非 void， 所以条件 true。不怕死可以这么玩来省行数。

whatever: 任意名称接冒号，为标签 
//不用goto而是正常顺序，标签内的会执行吗？会。执行因为标签只是一个跳转提示符


switch 是 if + goto 标签跳转表 的简写，此处 break 相当于在 表内结尾加了 goto end:





### ●. 函数内能不能 声明或定义临时函数？C++不行？ 其他语言可以不？

C++ 标准写法（Old School）：不行。你不能在 void do1() 里写 void innerFunc() {}。

C++ 现代写法（C++11 及以后）：可以！用 Lambda 表达式。

C++
void do1() {
    auto innerFunc = []() { printf("我是内部函数"); };
    innerFunc(); // 调用
}
其他语言：Python、JavaScript、Go、Rust、Pascal 都支持直接在函数里定义命名函数。C 语言（标准）不支持，但 GCC 编译器有扩展支持（不通用）。



 ### ●. 怎么写宏
// 在预处理器看来，一个宏定义必须写在一行里。如果你想换行写（为了人类阅读方便），
// 就必须在行尾加 \，告诉预处理器：“下一行其实跟这一行是连着的，别断开。”
// 注意：\ 后面绝对不能有任何空格，否则编译器会报错。
#define DEBUG_PRINT(x) \
do { \
  if (DEBUG_ON && S_DEBUG) \
  S_DEBUG->print(x); \
} while (0)
//用 do{}while(0)包裹是为了安全性，{}把if包裹在里面，while(0)后面必须接；分号，让其变成一个独立的语句块
### 主要用于 在简写 if(?)...else...中的时候
if (is_error)
    DEBUG_PRINT("Error occurred!");
else
    system_restart(); // 本意是：如果没有错误，就重启系统

if (is_error)
    if (DEBUG_ON && S_DEBUG)
        S_DEBUG->print("Error occurred!");❌️
    else                // 变成追随if (DEBUG_ON && S_DEBUG)了
        system_restart(); 

    if (is_error)
        if (DEBUG_ON && S_DEBUG)
            {S_DEBUG->print("Error occurred!");};❌️
else                    // 前面有{}+; 找不到前面的if了
    system_restart(); 




### ●. 宏的“膨胀”问题

- **宏会增加代码量吗？**：**会。** 宏是文本替换。如果你宏里写了 100 行代码，调用 1 万次，预处理器就会把这 100 行代码往你的 `.cpp` 里拷贝 1 万次。这会导致生成的二进制文件（`.exe` 或 `.bin`）体积迅速变大。
- **编译器会优化吗？**：编译器会尝试“指令合并”，但效果有限。如果宏很大，它无法像函数那样通过“调用”来节省空间。
- **换成 `const` 或 `inline` 函数更好吗？**：
  - 如果是简单的值，用 `const` 或 `constexpr`。
  - 如果是逻辑块，用 `inline` 函数。
  - **严谨建议**：现代 C++ 强烈建议用 `inline` 函数代替复杂的宏。`inline` 既有宏的高性能（避免函数调用开销），又有函数的类型安全和空间优化。

- 如果宏只是一个数字（如 `#define PI 3.14159`），调用 1W 次**完全没问题**，因为它只是操作数，不增加指令数量。
- 如果宏是一段复杂的逻辑（如刚才那个 `do...while(0)` 里的 `print`），调用 1W 次会使二进制文件显著膨胀。


  

### ●.  static 的作用
| **位置**   | **作用域 (可见性)** | **生命周期**       | **严谨理解**           |
| ---------- | ------------------- | ------------------ | ---------------------- |
| **函数外** | 仅限本 .cpp         | 整个程序运行期     | 防止重名冲突，局部化   |
| **函数内** | 仅限本函数          | **整个程序运行期** | 具有记忆能力的局部变量 |



### ●. 标签、跳转表、内存地址：它们的关系

我们得把这三个概念排个序：

1. **标签 (Label)**：这是你在**源码**层面的占位符。它在编译时会被转换成一个具体的**内存偏移量**。
2. **内存地址**：这是物理真相。不论是 `goto` 还是 `switch`，在 CPU 眼里都是 `JMP 0x00401234`（跳转到某个内存地址）。
3. **跳转表 (Jump Table)**：这是一个**优化手段**，不是所有的标签都会变成跳转表。
   - 如果你只有一两个 `goto`，编译器直接把地址写在指令里。
   - 如果你有一个 `switch` 且 case 很多且连续（比如 1~100），编译器为了不让你跑 100 次 `if` 判断，会专门开辟一块连续内存，
   - 像存数组一样存下 100 个标签的地址。这块“地址数组”才叫**跳转表**。




   
switch 展开为 goto 
switch (x) {    // 注意：switch 不能接受 &x (指针)，除非是指针的 switch，这里假设是 int x
    case 1:     //此处只能是常量，不能是 条件判断 或 函数 等任何其他，除非条件判断两端都是常量（被优化）
        doA();  //如果switch后啥也不写，case后写条件，那其实应该用if(?) goto 标签:
        x = 3;
        // 没 break → 会继续执行 case 3?
    case 2:
        doB();
        break;  // 退出 switch
    case 3:
        doC();
        break;
    default:
        docD();
        //不用加 break
}

// 等价于 goto：
if (x == 1) goto case1;
if (x == 2) goto case2;
if (x == 3) goto case3;
goto dft;
goto end;

case1:
    doA();
    x = 3;
case2:  // case1 会 fall through 到这里！
    doB();
    goto end;
case3:
    doC();
    goto end;
dft:
    docD();
end:
    //do nothing






核、最簡潔的方式幫你釘死這四個概念，順便清一下類型名稱的混亂。

0. 類型名大亂鬥（其實都是換湯不換藥）

- **`byte`**: Arduino 專屬定義，本質就是 **`uint8_t`** (0~255)。
- **`uchar` / `unsigned char`**: C 語言標準，本質也是 **`uint8_t`**。
- **`uint8_t`**: 最專業、最準確的寫法（代表 無符號 8 位元 整數）。
- **結論**：看到這三個，腦子裡直接把它們等同於 **「1 個字節的內存格子」** 就對了。

------

1. 為什麼 Buf 一定要用 `uint8_t` (uchar)？

- **物理對齊**：內存的最小跳轉單位就是 1 Byte。用 `uint8_t` 能保證 `buf[i]` 指向的就是第 `i` 個字節。
- **協議安全**：通訊協議（如 MAVLink）常有 `0xFD` (253) 這種大於 127 的數值。用有符號的 `int8_t` 會把它當成負數，導致邏輯判斷出錯。
- 堆疊增長高低，影響數組索引嗎？

2. 堆疊增長高低，影響數組索引嗎？
- **不影響**。
- **真相**：堆疊（Stack）往哪裡長是 CPU 函數調用的事。但 **數組（Array）** 永遠是 **「低地址往高地址」** 增長的。`buf[0]` 永遠在內存地址最小的地方，`buf[1]` 在它後面。
- 大小端序（Endianness）的影響範圍？

3. 大小端序（Endianness）的影響範圍？
- **不影響 Byte**：單個字節沒有順序問題。
- **不影響數組索引**：`buf[0]` 永遠是收到的第一個字節。
- **影響多字節轉換**：當你想把 `buf[0]` 和 `buf[1]` 拼成一個 `int16_t` 時，大端和小端會決定誰是高位、誰是低位。
- **與堆疊無關**：大小端是數據在格子裡的擺放方式，堆疊是格子怎麼疊放。
- `memcpy` 的本質？

4. `memcpy` 的本質？
- **不管類型**：它只收 `void*` 指針，眼中只有「要搬幾個 Byte」。
- **不管端序**：它只是物理搬運工，把 A 處的格子原封不動搬到 B 處。它不會幫你翻轉字節順序。

5. 一个数据包协议可能规定了小端序如MAVLINK
- ESP32 / STM32 / x86 都是小端序，非Byte数据直接 memcpy 就对了。
- 如果是大端序的芯片，对于int16_t, float 等，得先转换成小端序再 memcpy 打包进 MAVLINK
------



Serial.print(F("Hello World!")); // 直接把字符串常量放在 Flash，节省 RAM，但是读取速度慢。
已经用uart_driver_install初始化，不兼容uart_write_bytes
不加 F()：代码清爽，兼容 printf，RAM 够用就没风险。
只有当你发现 RAM 剩余不到 50KB，或者你写了几百行像“网页 HTML 模板”那样巨大的静态文本时，才考虑它。


#endif


// NOLINTEND
// @formatter:on
// clang-format on