// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2019-2022 Second State INC

//===-- wasmedge/runtime/serializemgr.h - Serialization Manager definition ------------===//
//
// Part of the WasmEdge-Snapshot Project.
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

#include "host/wasi/wasimodule.h"
#include "host/wasi/environ.h"
#include "wasi/api.hpp"

// #include <boost/archive/text_oarchive.hpp>
// #include <boost/archive/text_iarchive.hpp>
// #include <boost/serialization/variant.hpp>
// #include <boost/serialization/split_free.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

namespace WasmEdge {
namespace Runtime {

class SerializationManager {
public:  
  // typedef boost::archive::text_iarchive InputArchive;
	// typedef boost::archive::text_oarchive OutputArchive;

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
  using Environ = Host::WASI::Environ;
  using funcArgs = Environ::funcArgs;

  static inline constexpr const uint64_t kPageSize = UINT64_C(65536);
  static inline constexpr const int64_t ZipFactor = 16;
  static inline std::string InputFilePath = "/dev/null";
  static inline std::string OutputFilePath = "test.meta";

  static inline uint64_t ModeFlag = 0;
  static inline Host::WasiModule *WasiMod;

  SerializationManager() = delete;
  SerializationManager(const std::string &Path)
    : ArchiveFilePath(Path) {}

public:
  void save(Runtime::StackManager &StackMgr, AST::InstrView::iterator PC,
            const Runtime::Instance::FunctionInstance *FromFuncPtr) {
    OFS.open(ArchiveFilePath);
    OutputArchive OA{OFS};
    save_stack(OA, StackMgr);
    save_global(OA, StackMgr);
    save_pc(OA, PC, FromFuncPtr);
    save_memory(OA, StackMgr);
    // save_environ(OA);
    OFS.close();
  }

  void load(Runtime::StackManager &StackMgr, AST::InstrView::iterator &PC,
            const Runtime::Instance::FunctionInstance *&FromFuncPtr) {
    IFS.open(ArchiveFilePath);
    InputArchive IA{IFS};
    load_stack(IA, StackMgr);
    load_global(IA, StackMgr);
    load_pc(IA, StackMgr, PC, FromFuncPtr);
    load_memory(IA, StackMgr);
    // load_environ(IA);
    IFS.close();
  }

protected:
  void save_int(OutputArchive &OA, uint32_t i) {
    OA << i;
  }

  uint32_t load_int(InputArchive &IA) {
    uint32_t i;
    IA >> i;
    return i;
  }

  template<typename T>
  void save_span(OutputArchive &OA, Span<const T> S) {
    OA << S.size();
    for(uint32_t i = 0; i < S.size(); i++) {
      OA << S[i];
    }
  }

  template<typename T>
  Span<const T> load_span(InputArchive &IA) {
    uint32_t SpanSize;
    IA >> SpanSize;
    T* buffer = new T[SpanSize];
    for(uint32_t i = 0; i < SpanSize; i++) {
      IA >> buffer[i];
    }
    Span<const T> S(buffer, SpanSize);
    return S;
  }

  void save_stack(OutputArchive &OA, Runtime::StackManager &StackMgr) {
    // Save ValueStack size and elements.
    uint32_t StackSize = StackMgr.size();
    OA << StackSize;
    for (uint32_t i = 0; i < StackSize; i++) {
      save_value(OA, StackMgr.ValueStack[i]);
    }

    // Save Frame (except base frame and start frame). 
    StackSize = StackMgr.FrameStack.size();
    OA << StackSize;
    for (uint32_t i = 2; i < StackSize; i++) {
      Frame frame = StackMgr.FrameStack[i];
      OA << frame.Locals << frame.Arity << frame.VPos;
      save_pc(OA, frame.From, frame.FromFuncPtr);
    }
  }

  void load_stack(InputArchive &IA, Runtime::StackManager &StackMgr) {
    // Restore value stack elements.
    StackMgr.stackErase(StackMgr.size(), 0);
    uint32_t StackSize;
    IA >> StackSize;
    for (uint32_t i = 0; i < StackSize; i++) {
      StackMgr.push(load_value(IA));
    }

    // Load Frame.
    IA >> StackSize;
    // StackMgr.FrameStack.resize(StackSize);
    for (uint32_t i = 2; i < StackSize; i++) {
      Frame frame(StackMgr.getModule());
      IA >> frame.Locals >> frame.Arity >> frame.VPos;
      load_pc(IA, StackMgr, frame.From, frame.FromFuncPtr);
      StackMgr.FrameStack.push_back(frame);
    }
  }

  void save_global(OutputArchive &OA, Runtime::StackManager &StackMgr) {
    const auto *ModInst = StackMgr.getModule();

    // Save global elements.
    uint32_t GlobalNum = ModInst->getGlobalNum();
    OA << GlobalNum;
    for (uint32_t i = 0; i < GlobalNum; i++) {
      save_value(OA, (*ModInst->getGlobal(i))->getValue());
    }
  }

  void load_global(InputArchive &IA, Runtime::StackManager &StackMgr) {
    const auto *ModInst = StackMgr.getModule();

    // Restore global elements.
    uint32_t GlobalNum;
    IA >> GlobalNum;
    for (uint32_t i = 0; i < GlobalNum; i++) {
      ModInst->unsafeGetGlobal(i)->getValue() = load_value(IA);
    }
  }

  void save_memory(OutputArchive &OA, Runtime::StackManager &StackMgr) {
    const auto *ModInst = StackMgr.getModule();
    if (!ModInst->MemInsts.size()) return ;
    
    // Save memory as uint8 elements.
    auto Mem = ModInst->unsafeGetMemory(0);
    auto MemType = Mem->getMemoryType();
    uint64_t MemPages = MemType.getLimit().getMin();
    uint64_t ElemNum = MemPages * (kPageSize / sizeof(uint8_t));
    uint8_t *DataPtr = Mem->getDataPtr();
    OA << ElemNum;
    if (ElemNum == 0) return ;

    // Compress identical elements.
    int64_t count = 1;
    std::vector<uint32_t> tempVec(1, DataPtr[0]);

    // Compress identical elements of length "count".
    auto Dispatch = [&] (uint64_t i) {
      uint64_t length = tempVec.size() - count;
      if (length) {
        OA << length;
        for(uint64_t j = 0; j < length; j++) {
          OA << uint32_t(tempVec[j]);
        }
      }
      if (count) {
        OA << -count << uint32_t(DataPtr[i - 1]);
        // std::cerr << DataPtr[i - 1] << "\n";
      }
      tempVec.resize(0);
    };

    for (uint64_t i = 1; i < ElemNum; i++) {
      if (DataPtr[i] == DataPtr[i - 1]) {
        count += 1;
      } else {
        if (count >= ZipFactor) {
          Dispatch(i);
        }
        count = 1;
      }
      tempVec.push_back(DataPtr[i]);
    }
    if (count < ZipFactor) count = 0;
    Dispatch(ElemNum);
    OA << 0;
  }

  void load_memory(InputArchive &IA, Runtime::StackManager &StackMgr) {
    const auto *ModInst = StackMgr.getModule();
    if (!ModInst->MemInsts.size()) return ;

    // Load uint8 elements to buffer.
    auto Mem = ModInst->unsafeGetMemory(0);
    auto MemType = Mem->getMemoryType();
    uint64_t MemPages = MemType.getLimit().getMin();
    uint8_t *DataPtr = Mem->getDataPtr();
    uint64_t ElemNum;
    IA >> ElemNum;    
    uint8_t *Buffer = new uint8_t[ElemNum];
    
    // Parse uint8 elements from compression.
    uint64_t index = 0;
    int64_t count;
    IA >> count;
    while (count) {
      if (count > 0) {
        for (int64_t i = 0; i < count; i++) {
          uint32_t tmp;
          IA >> tmp;
          Buffer[index++] = tmp;
        }
      } else {
        uint32_t x;
        IA >> x;
        for (int64_t i = 0; i < -count; i++) {
          Buffer[index++] = x;
        }
      }
      IA >> count;
    }
    
    // Check if page growing is needed.
    if (ElemNum / (kPageSize / sizeof(uint8_t)) > MemPages) {
      Mem->growPage(ElemNum / (kPageSize / sizeof(uint8_t)) - MemPages);
    }

    // Copy buffer to memory.
    for (uint32_t i = 0; i < ElemNum; i++) {
      DataPtr[i] = Buffer[i];
    }
    delete[] Buffer;
  }

  void save_pc(OutputArchive &OA, 
               AST::InstrView::iterator PC, 
               const Runtime::Instance::FunctionInstance *FromFuncPtr) {

    AST::InstrView::iterator StartIt = FromFuncPtr->getInstrs().begin();
  
    // Save PC by recording Function ID and Instrustion ID.
    uint32_t FuncId = FromFuncPtr->FuncId;
    uint32_t InstrId = PC - StartIt;
    OA << FuncId << InstrId;
  }

  void load_pc(InputArchive &IA,
               Runtime::StackManager &StackMgr, 
               AST::InstrView::iterator &PC, 
               const Runtime::Instance::FunctionInstance *&FromFuncPtr) {
      
      const auto *ModInst = StackMgr.getModule();
    
      uint32_t FuncId;
      uint32_t InstrId;
      IA >> FuncId >> InstrId;
    
      FromFuncPtr = ModInst->FuncInsts[FuncId];
      AST::InstrView::iterator StartIt = FromFuncPtr->getInstrs().begin();
      PC = StartIt + InstrId;
    }

  /*
  void save_environ(OutputArchive &OA) {
    auto& Env = WasiMod->getEnv();
    auto& LogName = Env.LogName;
    auto& LogArgs = Env.LogArgs;

    assert(LogName.size() == LogArgs.size());
    uint32_t LogSize = LogName.size();
    OA << LogSize;
    for (uint32_t i = 0; i < LogSize; i++) {
      OA << LogName[i];
      switch (LogName[i])
      {
      case Environ::funcName::sockOpen:
        OA << LogArgs[i].sockOpen.AddressFamily;
        OA << LogArgs[i].sockOpen.SockType;
        OA << LogArgs[i].sockOpen.ReFd;
        break;
      
      case Environ::funcName::sockBind:
        OA << LogArgs[i].sockBind.Fd;
        OA << LogArgs[i].sockBind.AddressFamily;
        save_span<uint8_t>(OA, LogArgs[i].sockBind.Address);
        OA << LogArgs[i].sockBind.Port;
        break;

      case Environ::funcName::sockConnect:
        OA << LogArgs[i].sockConnect.Fd;
        OA << LogArgs[i].sockConnect.AddressFamily;
        save_span<uint8_t>(OA, LogArgs[i].sockConnect.Address);
        OA << LogArgs[i].sockConnect.Port;
        break;
      }
    }
  }

  void load_environ(InputArchive &IA) {
    auto& Env = WasiMod->getEnv();
    auto& LogName = Env.LogName;
    auto& LogArgs = Env.LogArgs;

    uint32_t LogSize;
    IA >> LogSize;
    LogName.clear();
    LogArgs.clear();
    for (uint32_t i = 0; i < LogSize; i++) {
      Environ::funcName curFuncName;
      IA >> curFuncName;
      switch (curFuncName)
      {
      case Environ::funcName::sockOpen:
        {
          __wasi_address_family_t AddressFamily;
          __wasi_sock_type_t SockType;
          __wasi_fd_t ReFd;
          IA >> AddressFamily;
          IA >> SockType;
          IA >> ReFd;
          // funcArgs args = {.sockOpen = {AddressFamily, SockType, ReFd}};
          Env.sockReOpen(AddressFamily, SockType, ReFd);
          break;
        }
      
      case Environ::funcName::sockBind:
        {
          __wasi_fd_t Fd;
          __wasi_address_family_t AddressFamily;
          Span<const uint8_t> Address;
          uint16_t Port;
          IA >> Fd;
          IA >> AddressFamily;
          Address = load_span<uint8_t>(IA);
          IA >> Port;
          // funcArgs args = {.sockBind = {Fd, AddressFamily, Address, Port}};
          Env.sockBind(Fd, AddressFamily, Address, Port);
          break;
        }

      case Environ::funcName::sockConnect:
        {
          __wasi_fd_t Fd;
          __wasi_address_family_t AddressFamily;
          Span<const uint8_t> Address;
          uint16_t Port;
          IA >> Fd;
          IA >> AddressFamily;
          Address = load_span<uint8_t>(IA);
          IA >> Port;
          // funcArgs args = {.sockConnect = {Fd, AddressFamily, Address, Port}};
          Env.sockConnect(Fd, AddressFamily, Address, Port);
          break;
        }
      }
    }
  }
  */

private:
  std::string ArchiveFilePath;
  std::ifstream IFS;
  std::ofstream OFS;

  std::pair<uint64_t,uint64_t> uint128_t_encode(uint128_t src) {
  #if defined(__x86_64__) || defined(__aarch64__) || \
    (defined(__riscv) && __riscv_xlen == 64)
    constexpr const uint128_t bottom_mask = (uint128_t{1} << 64) - 1;
    constexpr const uint128_t top_mask = ~bottom_mask;
    return { src & bottom_mask, (src & top_mask) >> 64 };
  #else
    return { V.high(), V.low() };
  #endif
  }

  uint128_t uint128_t_decode(uint64_t src1, uint64_t src2) {
  #if defined(__x86_64__) || defined(__aarch64__) || \
    (defined(__riscv) && __riscv_xlen == 64)
    return (uint128_t{src2} << 64) | src1;
  #else
    return uint128_t{ V.high(), V.low() };
  #endif
  }

  // Save value as 64-bit literial. (Instead of 128-bit)
  // Remember: SIMD operator should not be used.
  void save_value(OutputArchive &OA, Value &V) {
    auto ValuePair = uint128_t_encode(V.get<uint128_t>());
    OA << ValuePair.first;
  }

  Value load_value(InputArchive &IA) {
    uint64_t src = 0;
    IA >> src;
    return Value{src};
  }
};

} // namespace Runtime
} // namespace WasmEdge


/*

namespace boost { 
namespace serialization {

using WasmEdge::ValVariant;
using WasmEdge::uint128_t;

template<class Archive>
void save(Archive & ar, const uint128_t & Val, unsigned int version) {
  std::ignore = version;
  uint128_t src = Val;//.get<uint128_t>();
  #if defined(__x86_64__) || defined(__aarch64__) || \
    (defined(__riscv) && __riscv_xlen == 64)
    constexpr const uint128_t bottom_mask = (uint128_t{1} << 64) - 1;
    constexpr const uint128_t top_mask = ~bottom_mask;
    ar << (src & bottom_mask);
    ar << ((src & top_mask) >> 64);
  #else
    ar << (V.high());
    ar << (V.low());
  #endif
}

template<class Archive>
void load(Archive & ar, uint128_t & Val, unsigned int version) {
  std::ignore = version;
  uint64_t src1, src2;
  ar >> src1 >> src2;
  #if defined(__x86_64__) || defined(__aarch64__) || \
    (defined(__riscv) && __riscv_xlen == 64)
    Val = (uint128_t{src2} << 64) | src1;
  #else
    Val = uint128_t{ V.high(), V.low() };
  #endif
}


} //namespace serialization 
} // namespace boost

BOOST_SERIALIZATION_SPLIT_FREE(ValVariant)

*/