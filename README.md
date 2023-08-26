### 基于 wasmedge 的链外计算执行节点

本仓库基于 wasmedge 解释器实现了 wasm 运行时快照保存、恢复、验证功能。

#### 编译与运行

编译 wasmedge 项目的方式如下：

```bash
# 更新子模块 boost_c++
git submodule update --init

# 创建 build 文件夹并编译
mkdir -p build && cd build
make -j
```

使用命令行调起执行节点的方式如下：

```bash
./build/tools/wasmedge/wasmedge --enable-snapshot \
                                --snapshot-input [输入快照路径] \
                                --snapshot-output [输出快照路径] \
                                --gas-limit [单次运行费用限制]
                                [wasm程序路径]
								
# --snapshot-input 参数为可选，如果不含该参数代表程序从头开始运行
# 如果运行费用充足，则不生成输出快照
# 关于如何自定义 opcode 费用见下一部分
```

仓库同时提供了一个使用 golang 调起执行节点的示例，位于 `/snapcaller/` 路径下；
  - `snapcaller.go` 提供了两个方法 `WasmRun`、`WasmValidate`，具体使用方法见代码注释。

#### 代码功能指南

主要介绍 wasmedge 项目中添加的快照功能所涉及的部分。

如果希望深入代码修改的具体细节，可以使用 github 的分支对比功能 [对比](https://github.com/WasmEdge/WasmEdge/compare/master...fanfaredash:WasmEdge:master#diff-a664e8a9fe3fece55102e483bd1a38c8154911d289f64f981e599809b4504fb5) 本仓库与 wasmedge 原项目分支。

##### wasm 执行器部分代码

`lib/executor/engine/engine.cpp`：wasm 解释器执行核心，包含核心函数 runFunction，execute
  - `runFunction`：处理 wasm 程序首次进入 _start 函数时的操作。
  - `execute`：逐字节码解析 wasm 程序，并执行相应功能，计算费用。

`lib/executor/helper.cpp`：包含函数 enterfunction，维护函数调用时栈帧。

`include/runtime/stackmgr.h`：包含核心类 StackManager，维护栈帧 FrameStack 与运算栈 ValueStack。

##### 命令行参数解析部分代码

`include/driver/tool.h`：添加了快照相关的命令行参数。

`lib/driver/runtimeTool.cpp`：解析完命令行参数后进入这里，然后调起 wasm 解释器执行 wasm 程序。

##### 序列化部分代码

`include/runtime/serializemgr.h`：
  - 包含序列化核心类 SerializeManager，基于 boost_c++ 实现。
  - 主要保存栈、全局变量、申请的内存、以及 PC。
  - 需要注意 PC 的记录方式：解释器会把每个 wasm 函数的字节码按顺序保存；
    所以序列化的过程中保存了函数的 ID 与指令的 ID，方便获取对应 PC 值。

- SerializeManager 在 `lib/executor/engine/engine.cpp` 中被实例化使用。

##### 字节码费用定义部分代码

`include/common/statistics.h`：CostTabDefault 数组规定了每个字节码消耗的费用。
  - 字节码功能参考资料：https://pengowray.github.io/wasm-ops/

