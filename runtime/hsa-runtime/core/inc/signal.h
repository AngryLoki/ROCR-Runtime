////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2014-2015, Advanced Micro Devices, Inc. All rights reserved.
//
// Developed by:
//
//                 AMD Research and AMD HSA Software Development
//
//                 Advanced Micro Devices, Inc.
//
//                 www.amd.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal with the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
//  - Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimers.
//  - Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimers in
//    the documentation and/or other materials provided with the distribution.
//  - Neither the names of Advanced Micro Devices, Inc,
//    nor the names of its contributors may be used to endorse or promote
//    products derived from this Software without specific prior written
//    permission.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS WITH THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////

// HSA runtime C++ interface file.

#ifndef HSA_RUNTME_CORE_INC_SIGNAL_H_
#define HSA_RUNTME_CORE_INC_SIGNAL_H_

#include "hsakmt.h"

#include "core/common/shared.h"

#include "core/inc/runtime.h"
#include "core/inc/checked.h"

#include "core/util/utils.h"
#include "core/util/locks.h"

#include "inc/amd_hsa_signal.h"

namespace core {
class Agent;
class Signal;

/// @brief ABI and object conversion struct for signals.  May be shared between processes.
struct SharedSignal {
  amd_signal_t amd_signal;
  Signal* core_signal;
  Check<0x71FCCA6A3D5D5276, true> id;

  bool IsValid() const { return id.IsValid(); }

  SharedSignal() {
    amd_signal.kind = AMD_SIGNAL_KIND_INVALID;
    core_signal = nullptr;
  }

  static __forceinline SharedSignal* Convert(hsa_signal_t signal) {
    if (signal.handle == 0) throw std::bad_cast();
    SharedSignal* ret = reinterpret_cast<SharedSignal*>(static_cast<uintptr_t>(signal.handle) -
                                                        offsetof(SharedSignal, amd_signal));
    if (!ret->IsValid()) throw std::bad_cast();
    return ret;
  }
};
static_assert(std::is_standard_layout<SharedSignal>::value,
              "SharedSignal must remain standard layout for IPC use.");
static_assert(std::is_trivially_destructible<SharedSignal>::value,
              "SharedSignal must not be modified on delete for IPC use.");

class LocalSignal {
 public:
  explicit LocalSignal(hsa_signal_value_t initial_value) {
    if (!local_signal_.IsSharedObjectAllocationValid()) throw std::bad_alloc();
    local_signal_.shared_object()->amd_signal.value = initial_value;
  }

  SharedSignal* signal() const { return local_signal_.shared_object(); }

 private:
  Shared<SharedSignal, AMD_SIGNAL_ALIGN_BYTES> local_signal_;
};

/// @brief An abstract base class which helps implement the public hsa_signal_t
/// type (an opaque handle) and its associated APIs. At its core, signal uses
/// a 32 or 64 bit value. This value can be waitied on or signaled atomically
/// using specified memory ordering semantics.
class Signal {
 public:
  /// @brief Constructor Links and publishes the signal interface object.
  explicit Signal(SharedSignal* abi_block)
      : signal_(abi_block->amd_signal), async_copy_agent_(NULL) {
    if (abi_block == nullptr) throw std::bad_alloc();

    invalid_ = false;
    waiting_ = 0;
    retained_ = 0;

    abi_block->core_signal = this;
  }

  virtual ~Signal() { invalid_ = true; }

  bool IsValid() const { return !invalid_; }

  /// @brief Converts from this implementation class to the public
  /// hsa_signal_t type - an opaque handle.
  static __forceinline hsa_signal_t Convert(Signal* signal) {
    assert(signal != nullptr && signal->IsValid() && "Conversion on invalid Signal object.");
    const uint64_t handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&signal->signal_));
    const hsa_signal_t signal_handle = {handle};
    return signal_handle;
  }

  /// @brief Converts from this implementation class to the public
  /// hsa_signal_t type - an opaque handle.
  static __forceinline const hsa_signal_t Convert(const Signal* signal) {
    assert(signal != nullptr && signal->IsValid() && "Conversion on invalid Signal object.");
    const uint64_t handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&signal->signal_));
    const hsa_signal_t signal_handle = {handle};
    return signal_handle;
  }

  /// @brief Converts from public hsa_signal_t type (an opaque handle) to
  /// this implementation class object.
  static __forceinline Signal* Convert(hsa_signal_t signal) {
    SharedSignal* shared = SharedSignal::Convert(signal);
    if (shared->core_signal != nullptr)
      return shared->core_signal;
    else
      throw std::bad_cast();
  }

  // Below are various methods corresponding to the APIs, which load/store the
  // signal value or modify the existing signal value automically and with
  // specified memory ordering semantics.
  virtual hsa_signal_value_t LoadRelaxed() = 0;
  virtual hsa_signal_value_t LoadAcquire() = 0;

  virtual void StoreRelaxed(hsa_signal_value_t value) = 0;
  virtual void StoreRelease(hsa_signal_value_t value) = 0;

  virtual hsa_signal_value_t WaitRelaxed(hsa_signal_condition_t condition,
                                         hsa_signal_value_t compare_value,
                                         uint64_t timeout,
                                         hsa_wait_state_t wait_hint) = 0;
  virtual hsa_signal_value_t WaitAcquire(hsa_signal_condition_t condition,
                                         hsa_signal_value_t compare_value,
                                         uint64_t timeout,
                                         hsa_wait_state_t wait_hint) = 0;

  virtual void AndRelaxed(hsa_signal_value_t value) = 0;
  virtual void AndAcquire(hsa_signal_value_t value) = 0;
  virtual void AndRelease(hsa_signal_value_t value) = 0;
  virtual void AndAcqRel(hsa_signal_value_t value) = 0;

  virtual void OrRelaxed(hsa_signal_value_t value) = 0;
  virtual void OrAcquire(hsa_signal_value_t value) = 0;
  virtual void OrRelease(hsa_signal_value_t value) = 0;
  virtual void OrAcqRel(hsa_signal_value_t value) = 0;

  virtual void XorRelaxed(hsa_signal_value_t value) = 0;
  virtual void XorAcquire(hsa_signal_value_t value) = 0;
  virtual void XorRelease(hsa_signal_value_t value) = 0;
  virtual void XorAcqRel(hsa_signal_value_t value) = 0;

  virtual void AddRelaxed(hsa_signal_value_t value) = 0;
  virtual void AddAcquire(hsa_signal_value_t value) = 0;
  virtual void AddRelease(hsa_signal_value_t value) = 0;
  virtual void AddAcqRel(hsa_signal_value_t value) = 0;

  virtual void SubRelaxed(hsa_signal_value_t value) = 0;
  virtual void SubAcquire(hsa_signal_value_t value) = 0;
  virtual void SubRelease(hsa_signal_value_t value) = 0;
  virtual void SubAcqRel(hsa_signal_value_t value) = 0;

  virtual hsa_signal_value_t ExchRelaxed(hsa_signal_value_t value) = 0;
  virtual hsa_signal_value_t ExchAcquire(hsa_signal_value_t value) = 0;
  virtual hsa_signal_value_t ExchRelease(hsa_signal_value_t value) = 0;
  virtual hsa_signal_value_t ExchAcqRel(hsa_signal_value_t value) = 0;

  virtual hsa_signal_value_t CasRelaxed(hsa_signal_value_t expected,
                                        hsa_signal_value_t value) = 0;
  virtual hsa_signal_value_t CasAcquire(hsa_signal_value_t expected,
                                        hsa_signal_value_t value) = 0;
  virtual hsa_signal_value_t CasRelease(hsa_signal_value_t expected,
                                        hsa_signal_value_t value) = 0;
  virtual hsa_signal_value_t CasAcqRel(hsa_signal_value_t expected,
                                       hsa_signal_value_t value) = 0;

  //-------------------------
  // implementation specific
  //-------------------------
  typedef void* rtti_t;

  /// @brief Returns the address of the value.
  virtual hsa_signal_value_t* ValueLocation() const = 0;

  /// @brief Applies only to InterrupEvent type, returns the event used to.
  /// Returns NULL for DefaultEvent Type.
  virtual HsaEvent* EopEvent() = 0;

  /// @brief Waits until any signal in the list satisfies its condition or
  /// timeout is reached.
  /// Returns the index of a satisfied signal.  Returns -1 on timeout and
  /// errors.
  static uint32_t WaitAny(uint32_t signal_count, const hsa_signal_t* hsa_signals,
                          const hsa_signal_condition_t* conds, const hsa_signal_value_t* values,
                          uint64_t timeout_hint, hsa_wait_state_t wait_hint,
                          hsa_signal_value_t* satisfying_value);

  __forceinline bool IsType(rtti_t id) { return _IsA(id); }

  /// @brief Allows special case interaction with signal destruction cleanup.
  void Retain() { atomic::Increment(&retained_); }
  void Release() { atomic::Decrement(&retained_); }

  /// @brief Checks if signal is currently in use such that it should not be
  /// deleted.
  bool InUse() const { return (retained_ != 0) || (waiting_ != 0); }

  /// @brief Checks if signal is currently in use by a wait API.
  bool InWaiting() const { return waiting_ != 0; }

  __forceinline void async_copy_agent(core::Agent* agent) {
    async_copy_agent_ = agent;
  }

  __forceinline core::Agent* async_copy_agent() { return async_copy_agent_; }

  /// @brief Structure which defines key signal elements like type and value.
  /// Address of this struct is used as a value for the opaque handle of type
  /// hsa_signal_t provided to the public API.
  amd_signal_t& signal_;

 protected:
  /// @brief Simple RTTI type checking helper
  /// Returns true if the object can be converted to the query type via
  /// static_cast.
  /// Do not use directly.  Use IsType in the desired derived type instead.
  virtual bool _IsA(rtti_t id) const = 0;

  /// @variable  Indicates if signal is valid or not.
  volatile bool invalid_;

  /// @variable Indicates number of runtime threads waiting on this signal.
  /// Value of zero means no waits.
  volatile uint32_t waiting_;

  volatile uint32_t retained_;

  /// @variable Pointer to agent used to perform an async copy.
  core::Agent* async_copy_agent_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Signal);
};

struct hsa_signal_handle {
  hsa_signal_t signal;

  hsa_signal_handle() {}
  hsa_signal_handle(hsa_signal_t Signal) { signal = Signal; }
  operator hsa_signal_t() { return signal; }
  Signal* operator->() { return core::Signal::Convert(signal); }
};
static_assert(
    sizeof(hsa_signal_handle) == sizeof(hsa_signal_t),
    "hsa_signal_handle and hsa_signal_t must have identical binary layout.");
static_assert(
    sizeof(hsa_signal_handle[2]) == sizeof(hsa_signal_t[2]),
    "hsa_signal_handle and hsa_signal_t must have identical binary layout.");

class SignalGroup : public Checked<0xBD35DDDD578F091> {
 public:
  static __forceinline hsa_signal_group_t Convert(SignalGroup* group) {
    const hsa_signal_group_t handle = {static_cast<uint64_t>(reinterpret_cast<uintptr_t>(group))};
    return handle;
  }
  static __forceinline SignalGroup* Convert(hsa_signal_group_t group) {
    return reinterpret_cast<SignalGroup*>(static_cast<uintptr_t>(group.handle));
  }

  SignalGroup(uint32_t num_signals, const hsa_signal_t* signals);
  ~SignalGroup() { delete[] signals; }

  bool IsValid() const {
    if (CheckedType::IsValid() && signals != NULL) return true;
    return false;
  }

  const hsa_signal_t* List() const { return signals; }
  uint32_t Count() const { return count; }

 private:
  hsa_signal_t* signals;
  const uint32_t count;
  DISALLOW_COPY_AND_ASSIGN(SignalGroup);
};

}  // namespace core
#endif  // header guard
