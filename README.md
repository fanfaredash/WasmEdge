# 基于 wasmedge 的链外计算执行节点

基于 wasmedge 解释器实现了 wasm 运行时快照保存、恢复、验证功能。

## 1 编译与运行

### 1.1 代码编译

编译 wasmedge 项目的方式如下：

```bash
# 创建 build 文件夹并编译
mkdir -p build && cd build
make -j
```

编译完成后得到可执行文件与动态链接库路径如下：

```bash
./build/lib/api/libwasmedge.so.0.0.3
# WasmEdge 动态链接库

./build/tools/wasmedge/wasmedge
# WasmEdge 可执行文件
```

在运行执行节点的过程中，需要将动态链接库位置以环境变量的形式提供给程序。

### 1.2 代码运行

执行节点可以使用命令行工具调起执行，常用参数如下所示：

| 参数名                | 参数信息           | 备注                                                         |
| --------------------- | ------------------ | ------------------------------------------------------------ |
| `--snapshot-input`    | {输入快照目录路径} | 程序将从快照保存时刻开始运行；<br />如果不含该参数，则代表程序从头开始运行。<br />路径是一个字符串，对应快照所在目录；<br />路径字符串不应包含结尾的 `/`，形如 `./input` 而非 `./input/`； |
| `--snapshot-id`       | {输入快照ID}       | 程序将从快照保存时刻开始运行；<br />如果不含该参数，则代表程序从头开始运行。<br />快照由 ID 统一命名，相当于快照文件名，具体描述见后面章节。 |
| `--snapshot-output`   | {输出快照目录路径} | 程序将运行中间状态保存为快照输出至指定路径；<br />如果不含该参数，程序将输出至 `./snapshot`。<br />（且并不会主动创建 `./snapshot` 目录，可能会因为目录不存在报错）<br />路径是一个字符串，对应快照所在目录；<br />路径字符串不应包含结尾的 `/`，形如 `./input` 而非 `./input/`； |
| `--gas-limit`         | {单次运行费用限制} | 执行节点以字节码为单位统计运行费用；<br />当费用达到阈值时，会产生快照并终止程序。<br />（如果运行费用充足，则不生成输出快照） |
| `--enable-gas-refill` |                    | 若启用此参数，在执行费用达到阈值时，<br />会自动补充费用至初始值，可以不断产生快照直到程序结束。 |

 命令行工具运行示例如下所示：

```bash
LD_LIBRARY_PATH={动态链接库目录} \
./build/tools/wasmedge/wasmedge --snapshot-input {输入快照目录路径} \
																--snapshot-id {输入快照ID} \
                                --snapshot-output {输出快照目录路径} \
                                --gas-limit {单次运行费用限制} \
                                [--enable-gas-refill] \
                                {wasm程序路径} [wasm入口函数] {wasm程序参数}

# 推荐通过指定环境变量 LD_LIBRARY_PATH 的方式链接动态链接库，如 "./build/lib/api"
# --snapshot-input 参数为可选，如果不含该参数代表程序从头开始运行
# 如果运行费用充足，则不生成输出快照
# 大部分 wasm 程序运行不需要注明 wasm 入口函数，取决于 wasm 程序本身的编译方式
```

### 1.3 快照结构

#### 1.3.1 快照与增量内存存储

一次完整的执行生成的所有快照均位于 `--snapshot-output` 参数指定的路径，如下所示：

```bash
snapshot/
├── 1.bin
├── 1.snap
├── 2.bin
├── 2.snap
├── 3.bin
├── 3.snap
├── ...
└── result.txt
```

快照由 ID 编号。每个快照对应两个文件 `{id}.snap`，`{id}.bin` 。

- `{id}.snap`：包含快照 `{id}` 时刻除内存之外的动态信息。
- `{id}.bin`：增量内存存储，包含从快照 `{id-1}` 运行至快照 `{id}` 过程中产生的内存改动。

注意，由于快照使用增量保存的方式存储内存，所以想要恢复至某个中间快照状态，需要从最开始到该状态所有的 `.bin` 信息：

- 例如，如果程序想要恢复至快照 8 的状态，应该首先指定参数 `--snapshot-input ./input --snapshot-id 8`；
- 然后在指定的输入路径放入 `8.snap`，以及从 1 到 8 的所有 `bin` 文件：`1.bin`，`2.bin`，... `8.bin`。	

#### 1.3.2 快照与程序运行结果

程序运行结果记录在 `result.txt` ，一个文本文件中，它由 2～3 行构成，文件内容如下所示：

```
{程序运行状态}
{最新快照编号}
[程序返回值若干]
```

在运行结束后，无论程序是因费用不足而结束还是正常结束，都会生成 `result.txt`，包含程序运行结果。其中：

- 第一行是一个正整数，表示程序运行状态。如果是 0 说明是正常结束，反之说明是异常结束。
- 第二行是一个正整数，表示程序本次输出生成的最大的快照编号。
  - 例如，开启 `--enable-gas-refill` 参数时将依次生成全部快照，从 `1.snap` 生成到 `48.snap`，则最大编号为 48；
  - 又如，不开启 `--enable-gas-refill` 参数，`--snapshot-id` 参数为 12，则只会生成下一个快照，可能会生成 `13.snap`，此时最大编号为 13；
  - 又如，不开启 `--enable-gas-refill` 参数，`--snapshot-id` 参数为 48，则只会生成下一个快照，但费用足够使程序正常执行结束，此时最大编号仍为 48，但不产生快照，只产生 `result.txt`。
- 第三行是一个正整数，表示程序本次输出的返回值。如果是异常结束，则必然没有返回值。

### 1.4 程序示例

下面将以矩阵乘法程序 `./wasmfiles/matrix_multiplication_cpp_100.wasm` 为例展示程序的执行、快照保存与恢复功能。

#### 1.4.1 程序执行与快照保存

进入项目主目录，在编译完成后执行命令如下：

```bash
# 创建目录
mkdir ./input
mkdir ./output

# 运行矩阵乘法示例程序并生成快照
LD_LIBRARY_PATH="./build/lib/api" \
./build/tools/wasmedge/wasmedge --gas-limit 10000000 \
																--snapshot-output ./output \
																--enable-gas-refill \
																./wasmfiles/matrix_multiplication_cpp_100.wasm run
```

执行完成后观察 `./output/` 目录，发现运行生成了 10 个快照，从 `1.snap`，`1.bin` 到 `10.snap`，`10.bin`，以及 `result.txt`。

打开 `result.txt`，内容如下：

```
0
10
32835000
```

其中，第一行为 exit_status，代表执行正常结束，第二行为生成的快照编号至 10，第三行为人为设定的返回值。

#### 1.4.2 快照恢复与快照验证

基于 1.4.1 完成快照恢复，首先进入项目主目录，将快照文件放入 `./input` 目录，执行命令如下：

```bash
# 复制需要的快照文件
cd ./output
cp 1.bin 2.bin 3.bin 4.bin 4.snap ../input
cp 5.bin 5.snap result.txt ../input
cd ..

# 清空上次执行的结果
rm ./output/*

# 从快照4恢复示例程序并生成下一个快照
LD_LIBRARY_PATH="./build/lib/api" \
./build/tools/wasmedge/wasmedge --gas-limit 10000000 \
																--snapshot-input ./input \
																--snapshot-id 4 \
																--snapshot-output ./output \
																./wasmfiles/matrix_multiplication_cpp_100.wasm run
```

从快照 4 恢复示例程序需要以下文件：`1.bin`，`2.bin`，`3.bin`，`4.bin`，`4.snap`。

本次运行没有启用 `--enable-gas-refill` 参数，会执行到快照 5 并终止程序。

现在进行快照正确性验证：将本次运行执行得到的快照 5：`5.bin`，`5.snap` 与复制好的备份进行对比：

```bash
diff ./output/5.snap ./input/5.snap
diff ./output/5.bin ./input/5.bin
```

命令行没有输出，则说明正确性验证通过。

接下来启用 `--enable-gas-refill` 参数，进行最终结果的验证：

```bash
# 从快照4恢复示例程序并生成全部快照
LD_LIBRARY_PATH="./build/lib/api" \
./build/tools/wasmedge/wasmedge --gas-limit 10000000 \
																--snapshot-input ./input \
																--snapshot-id 4 \
																--snapshot-output ./output \
																--enable-gas-refill \
																./wasmfiles/matrix_multiplication_cpp_100.wasm run
```

运行完成后，将最终运行结果 `result.txt` 与复制好的备份进行对比：

```
diff ./output/result.txt ./input/result.txt
```

命令行没有输出，则说明正确性验证通过。

#### 1.4.3 更多示例

项目同时还提供了斐波那契数列计算程序 `./wasmfiles/fib32.wasm` 与 SHA-256 计算程序 `./wasmfiles/sha256.wasm` ：

```bash
# 由于wasm程序编写方法的不同，调用方式略有区别

# fib 程序入口函数名为 fib，接受一个正整数为参数，代表计算数列项数，返回值为对应项
LD_LIBRARY_PATH="./build/lib/api" \
./build/tools/wasmedge/wasmedge --gas-limit 1000000 \
																--snapshot-output ./output \
                                --enable-gas-refill \
																./wasmfiles/fib32.wasm fib 27

# sha256 程序无需提供入口函数，接受一个正整数为参数，代表计算的哈希长度，没有返回值
LD_LIBRARY_PATH="./build/lib/api" \
./build/tools/wasmedge/wasmedge --gas-limit 1000000 \
																--snapshot-output ./output \
                                --enable-gas-refill \
																./wasmfiles/sha256.wasm 65536
```


## 2 WASM 程序编写

## 3 代码功能指南

### wasm 执行器部分代码

`lib/executor/engine/engine.cpp`：wasm 解释器执行核心，包含核心函数 runFunction，execute

  - `runFunction`：处理 wasm 程序首次进入 _start 函数时的操作。
  - `execute`：逐字节码解析 wasm 程序，并执行相应功能，计算费用。

`lib/executor/helper.cpp`：包含函数 enterfunction，维护函数调用时栈帧。

`include/runtime/stackmgr.h`：包含核心类 StackManager，维护栈帧 FrameStack 与运算栈 ValueStack。

### 命令行参数解析部分代码

`include/driver/tool.h`：添加了快照相关的命令行参数。

`lib/driver/runtimeTool.cpp`：解析完命令行参数后进入这里，然后调起 wasm 解释器执行 wasm 程序。

### 序列化部分代码

`include/runtime/serializemgr.h`：

  - 包含序列化核心类 SerializeManager；
  - 主要保存栈、全局变量、申请的内存、以及 PC。
  - 需要注意 PC 的记录方式：解释器会把每个 wasm 函数的字节码按顺序保存；
    所以序列化的过程中保存了函数的 ID 与指令的 ID，方便获取对应 PC 值。

- SerializeManager 在 `lib/executor/engine/engine.cpp` 中被实例化使用。

### 字节码费用定义部分代码

`include/common/statistics.h`：CostTabDefault 数组规定了每个字节码消耗的费用。

  - 字节码功能参考资料：https://pengowray.github.io/wasm-ops/

## 核心技术点解析

- ...
