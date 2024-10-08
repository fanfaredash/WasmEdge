// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2019-2022 Second State INC

//===-- wasmedge/runtime/serializemgr.h - Serialization Manager definition ------------===//
//
// Part of the WasmEdge-Snap Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the definition of Serialization Manager.
///
//===----------------------------------------------------------------------===//
#pragma once


#include "runtime/stackmgr.h"
#include "runtime/instance/module.h"
#include "runtime/instance/memory.h"
#include "common/types.h"
#include "common/errinfo.h"
#include "common/spdlog.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>

namespace WasmEdge {
namespace Runtime {

class SerializationManager {

public:
    class OutputArchive {
    public:
        std::ofstream& out;

        OutputArchive(std::ofstream& outStream) : out(outStream) {}

        template<typename T>
        OutputArchive& operator<<(const T& value) {
            out << value << " ";  // 将值转换为文本形式并加入空格作为分隔符
            return *this;
        }
    };

    class InputArchive {
    public:
        std::ifstream& in;

        InputArchive(std::ifstream& inStream) : in(inStream) {}

        template<typename T>
        InputArchive& operator>>(T& value) {
            in >> value;  // 直接从文本中解析出相应类型的值
            return *this;
        }
    };

    using Value = ValVariant;
    using Frame = Runtime::StackManager::Frame;
    using Pointer = AST::InstrView::iterator;
    using Function = Runtime::Instance::FunctionInstance;

    // 单实例类相关函数
    static SerializationManager& getInstance() {
        static SerializationManager instance;
        return instance;
    }

    SerializationManager(const SerializationManager&) = delete;
    SerializationManager& operator=(const SerializationManager&) = delete;
    
    // 常数参数
    static inline constexpr const uint64_t kPageSize = UINT64_C(65536);
    static inline constexpr const int64_t ZipFactor = 16;

    // 公共参数
    static inline std::string InputDir = "/dev/null";
    static inline std::string OutputDir = "snapshot";
    static inline uint64_t UnitLimit = 5000000ull;
    static inline bool AutoRefill = false;
    static inline uint32_t SnapShotId = 0;
    static inline uint64_t GasCost = 0;
    static inline uint32_t MemPages = 0;

    void set_stack_manager(Runtime::StackManager *StackMgr) {
        this->StackMgr = StackMgr;
        this->ModInst = const_cast<Runtime::Instance::ModuleInstance *>(StackMgr->getModule());
    } 

    void open(std::string modeFlag) {
        ModeFlag = modeFlag;
        if (ModeFlag == "r") {
            // std::string path = InputDir + "/" + std::to_string(SnapShotId) + ".snap";
            IFS.open(InputDir + "/" + std::to_string(SnapShotId) + ".snap");
        } else if (ModeFlag == "w") {
            // std::string path = OutputDir + "/" + std::to_string(SnapShotId) + ".snap";
            OFS.open(OutputDir + "/" + std::to_string(SnapShotId) + ".snap");
        }
    }
    
    void close() {
        if (ModeFlag == "r") {
            IFS.close();
        } else if (ModeFlag == "w") {
            OFS.close();
        }
    }

    void save(Pointer PC) {
        OutputArchive OA{OFS};
        save_global(OA);
        save_stack(OA, PC);
        save_memory(OA);
    }

    void load(Pointer &PC, const Function *&F) {
        InputArchive IA{IFS};
        load_global(IA);
        load_stack(IA, PC, F);
        load_memory(IA);
  }

private:
    std::string ArchiveFilePath;
    std::ifstream IFS;
    std::ofstream OFS;
    std::string ModeFlag;

    // 程序运行信息
    Runtime::StackManager *StackMgr = nullptr;
    Runtime::Instance::ModuleInstance *ModInst;
    uint8_t *SavedData = nullptr;

    SerializationManager() {}
    
    // 基本的输入输出函数
    // void save_value(OutputArchive &OA, Value &V);
    // Value load_value(InputArchive &IA);

    // 代码段指针的输入输出函数
    // void save_pointer(OutputArchive &OA, uint32_t FuncId, Pointer P);
    // void load_pointer(InputArchive &IA, Pointer &P, const Function *&F);
    // Pointer load_pointer(InputArchive &IA);

    // 保存全局变量的函数
    // void save_global(OutputArchive &OA);
    // void load_global(InputArchive &IA);

    // 保存栈与PC的函数
    // void save_stack(OutputArchive &OA, Pointer PC);
    // void load_stack(InputArchive &IA, Pointer &PC, const Function *&F);

    // 内存增量存储函数
    // void save_changes_to_file(uint8_t* currentData, uint64_t elemNum, const std::string& filename);
    // void load_changes_from_file(uint8_t* data, uint64_t elemNum, const std::string& filename);

    // 保存内存的函数
    
    void save_memory(OutputArchive &OA) {
        if (ModInst->MemInsts.size() == 0) {
            OA << 0;
            save_changes_to_file(nullptr, 0, OutputDir + "/" + std::to_string(SnapShotId) + ".bin");
            return ;
        }
        auto Mem = ModInst->unsafeGetMemory(0);
        auto MemType = Mem->getMemoryType();
        uint64_t MemPages = MemType.getLimit().getMin();
        uint64_t ElemNum = MemPages * (kPageSize / sizeof(uint8_t));
        uint8_t *DataPtr = Mem->getDataPtr();
        OA << ElemNum;
        save_changes_to_file(DataPtr, ElemNum, OutputDir + "/" + std::to_string(SnapShotId) + ".bin");
    }

    void load_memory(InputArchive &IA) {
        uint64_t ElemNum;
        IA >> ElemNum;
        if (ElemNum == 0) {
            return ;
        } else if (ModInst->MemInsts.size() == 0) {
            // 自己搓一块内存进来
            ModInst->addMemory(AST::MemoryType(), uint32_t(1));
        }

        auto Mem = ModInst->unsafeGetMemory(0);
        auto MemType = Mem->getMemoryType();
        uint64_t MemPages = MemType.getLimit().getMin();
        uint8_t *DataPtr = Mem->getDataPtr();
        
        if (ElemNum / (kPageSize / sizeof(uint8_t)) > MemPages) {
            Mem->growPage(ElemNum / (kPageSize / sizeof(uint8_t)) - MemPages);
        }
        for (uint32_t i = 1; i <= SnapShotId; i++) {
            load_changes_from_file(DataPtr, ElemNum, InputDir + "/" + std::to_string(i) + ".bin");
        }
        if(SavedData != nullptr) delete []SavedData;
        SavedData = new uint8_t[ElemNum];
        memcpy(SavedData, DataPtr, ElemNum);
    }

    void save_changes_to_file(uint8_t* currentData, uint64_t elemNum, const std::string& filename) {
        // std::cout << "** Open file: " + filename << std::endl;
        spdlog::error("Open file: " + filename);
        std::ofstream outFile(filename, std::ios::binary);
        if (!outFile) {
            throw std::runtime_error("Failed to open file: " + filename);
        }
        // std::ofstream outFile2(filename + ".2", std::ios::binary);
        // for (uint64_t i = 0; i < elemNum; ++i) {
        //     outFile2.write(reinterpret_cast<const char*>(&currentData[i]), sizeof(uint8_t));
        // }
        // outFile2.close();
        // 保存更改部分的偏移和数据
        for (uint64_t i = 0; i < elemNum; ++i) {
            if (SavedData == nullptr || currentData[i] != SavedData[i]) {
                if (SavedData == nullptr && currentData[i] == 0) continue;
                // 写入偏移
                outFile.write(reinterpret_cast<const char*>(&i), sizeof(uint64_t));
                // 写入更改的数据
                outFile.write(reinterpret_cast<const char*>(&currentData[i]), sizeof(uint8_t));
            }
        }
        // 更新SavedData为当前的dataPtr
        if(SavedData != nullptr) delete []SavedData;
        if(elemNum == 0) {
            SavedData = nullptr;
        } else {
            SavedData = new uint8_t[elemNum];
            memcpy(SavedData, currentData, elemNum);
        } 
        outFile.close();
    }

    void load_changes_from_file(uint8_t* data, uint64_t elemNum, const std::string& filename) {
        std::ifstream inFile(filename, std::ios::binary);
        if (!inFile) {
            throw std::runtime_error("Failed to open file: " + filename);
        }
        uint64_t offset;
        uint8_t value;
        // 读取文件中的更改内容并应用到内存段
        while (inFile.read(reinterpret_cast<char*>(&offset), sizeof(uint64_t))) {
            inFile.read(reinterpret_cast<char*>(&value), sizeof(uint8_t));
            if (offset < elemNum) {
                data[offset] = value;
            }
        }
        inFile.close();
    }

    void save_stack(OutputArchive &OA, Pointer PC) {
        uint32_t StackSize = StackMgr->size();
        OA << StackSize;
        for (uint32_t i = 0; i < StackSize; i++) {
            save_value(OA, StackMgr->ValueStack[i]);
        }

        // for (uint32_t i = 0; i < StackSize; i++) {
        //     std::cerr << StackMgr->ValueStack[i].get<uint64_t>() << " ";
        // }
        // std::cerr << "\n";

        // 这里写一个离线。先把所有函数起点指针放进map里，然后二分
        std::map<Pointer, uint32_t> FuncMap;

        uint32_t FuncNum = ModInst->FuncInsts.size();        
        for (uint32_t FuncId = 0; FuncId < FuncNum; FuncId++) {
            FuncMap[ModInst->FuncInsts[FuncId]->getInstrs().begin()] = FuncId;
            // std::cout << "**** FuncId: " << FuncId 
            //           << "[" << ModInst->FuncInsts[FuncId]->getInstrs().begin() 
            //           << ", " << ModInst->FuncInsts[FuncId]->getInstrs().end()
            //           << "]" << std::endl;
        }

        StackSize = StackMgr->FrameStack.size();
        OA << StackSize;
        for (uint32_t i = 2; i < StackSize; i++) {
            Frame frame = StackMgr->FrameStack[i];
            OA << frame.Locals << frame.Arity << frame.VPos;

            Pointer P = frame.From;
            auto it = FuncMap.upper_bound(P);
            if (it != FuncMap.begin()) {
                --it;
                save_pointer(OA, it->second, P);

                // std::cout << "**** Frame: " << i
                //           << "| " << P
                //           << " / " << it->second
                //           << std::endl;

            } else {
                throw std::runtime_error("Frame pointer error: Function not found");
            }
        }

        auto it = FuncMap.upper_bound(PC);
        if (it != FuncMap.begin()) {
            --it;
            save_pointer(OA, it->second, PC);
        } else {
            throw std::runtime_error("PC error: Function not found");
        }
    }

    void load_stack(InputArchive &IA, Pointer &PC, const Function *&F) {
        uint32_t StackSize;
        IA >> StackSize;
        StackMgr->ValueStack.resize(StackSize);
        for (uint32_t i = 0; i < StackSize; i++) {
            StackMgr->ValueStack[i] = load_value(IA);
        }

        // for (uint32_t i = 0; i < StackSize; i++) {
        //     std::cerr << StackMgr->ValueStack[i].get<uint64_t>() << " ";
        // }
        // std::cerr << "\n";

        IA >> StackSize;
        for (uint32_t i = 2; i < StackSize; i++) {
            AST::InstrView::iterator From;
            uint32_t Locals;
            uint32_t Arity;
            uint32_t VPos;
            IA >> Locals >> Arity >> VPos;
            From = load_pointer(IA);
            Frame frame(ModInst, From, Locals, Arity, VPos);
            StackMgr->FrameStack.push_back(frame);
        }

        load_pointer(IA, PC, F);
    }

    void save_global(OutputArchive &OA) {
        uint32_t GlobalNum = ModInst->getGlobalNum();
        OA << GlobalNum;
        for (uint32_t i = 0; i < GlobalNum; i++) {
            save_value(OA, (*ModInst->getGlobal(i))->getValue());
        }
    }

    void load_global(InputArchive &IA) {
        uint32_t GlobalNum;
        IA >> GlobalNum;
        for (uint32_t i = 0; i < GlobalNum; i++) {
            ModInst->unsafeGetGlobal(i)->getValue() = load_value(IA);
        }
    }

    void save_pointer(OutputArchive &OA, uint32_t FuncId, Pointer P) {
        uint64_t InstrId = P - ModInst->FuncInsts[FuncId]->getInstrs().begin();
        OA << FuncId << InstrId;        
    }

    void load_pointer(InputArchive &IA, Pointer &P, const Function *&F) {
        uint32_t FuncId;
        uint64_t InstrId;
        IA >> FuncId >> InstrId;
        F = ModInst->FuncInsts[FuncId];
        P = F->getInstrs().begin() + InstrId;
    }

    Pointer load_pointer(InputArchive &IA) {
        uint32_t FuncId;
        uint64_t InstrId;
        IA >> FuncId >> InstrId;
        const Function *F = ModInst->FuncInsts[FuncId];
        return F->getInstrs().begin() + InstrId;
    }

    void save_value(OutputArchive &OA, Value &V) {
        OA << V.get<uint64_t>();
    }

    Value load_value(InputArchive &IA) {
        uint64_t src = 0;
        IA >> src;
        return Value{src};
    }

};


} // namespace Runtime
} // namespace WasmEdge
