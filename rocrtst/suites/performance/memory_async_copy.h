/*
 * =============================================================================
 *   ROC Runtime Conformance Release License
 * =============================================================================
 * The University of Illinois/NCSA
 * Open Source License (NCSA)
 *
 * Copyright (c) 2017, Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Developed by:
 *
 *                 AMD Research and AMD ROC Software Development
 *
 *                 Advanced Micro Devices, Inc.
 *
 *                 www.amd.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal with the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimers.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimers in
 *    the documentation and/or other materials provided with the distribution.
 *  - Neither the names of <Name of Development Group, Name of Institution>,
 *    nor the names of its contributors may be used to endorse or promote
 *    products derived from this Software without specific prior written
 *    permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS WITH THE SOFTWARE.
 *
 */

#ifndef ROCRTST_SUITES_PERFORMANCE_MEMORY_ASYNC_COPY_H_
#define ROCRTST_SUITES_PERFORMANCE_MEMORY_ASYNC_COPY_H_

#include <vector>
#include <algorithm>

#include "common/base_rocr.h"
#include "hsa/hsa.h"
#include "hsa/hsa_ext_amd.h"
#include "suites/test_common/test_base.h"

typedef enum TransType {H2D = 0, D2H, P2P} TransType;

typedef struct Transaction {
  int src;
  int dst;
  hsa_signal_t signal;
  size_t max_size;  // Max. amount of kBytes to copy
  TransType type;
  // BenchMark copy time
  std::vector<double> *benchmark_copy_time;
  // Min time
  std::vector<double> *min_time;
} Transaction;

class AgentInfo {
 public:
    AgentInfo(hsa_agent_t agent, int index, hsa_device_type_t device_type) {
      agent_ = agent;
      index_ = index;
      device_type_ = device_type;
    }
    AgentInfo() {}

    ~AgentInfo() {}
    hsa_agent_t agent(void) const {return agent_;}
    hsa_device_type_t device_type(void) const {return device_type_;}

    hsa_agent_t agent_;
    int index_;

 private:
    hsa_device_type_t device_type_;
};

class PoolInfo {
 public:
    PoolInfo(hsa_amd_memory_pool_t pool, int index,
               hsa_amd_segment_t segment, bool is_fine_graind, size_t size,
               AgentInfo *agent_info) {
      pool_ = pool;
      index_ = index;
      segment_ = segment;
      is_fine_grained_ = is_fine_graind;
      allocable_size_ = size;
      owner_agent_info_ = agent_info;
    }
    PoolInfo() {}
    ~PoolInfo() {}
    AgentInfo* owner_agent_info(void) const {return owner_agent_info_;}
    hsa_amd_memory_pool_t pool_;
    int index_;
    hsa_amd_segment_t segment_;
    bool is_fine_grained_;
    size_t allocable_size_;
 private:
    AgentInfo *owner_agent_info_;
};


// Used to print out topology info
typedef struct NodeInfo {
  AgentInfo agent;
  std::vector<PoolInfo> pool;
} NodeInfo;


class MemoryAsyncCopy : public TestBase {
 public:
  MemoryAsyncCopy();

  // @Brief: Destructor for test case of MemoryAsyncCopy
  virtual ~MemoryAsyncCopy();

  // @Brief: Setup the environment for measurement
  virtual void SetUp();

  // @Brief: Core measurement execution
  virtual void Run();

  // @Brief: Clean up and retrive the resource
  virtual void Close();

  // @Brief: Display  results
  virtual void DisplayResults() const;

  // There are 3 levels of testing, from quickest/very specific to
  // longest/most complete:
  // 1. to and from a specified source to a specified target
  // 2. to and from the cpu to 1 gpu, and to/from a gpu to another gpu
  //    (if available)
  // 3. to and from the cpu to 1 gpu and, to/from every gpu to every
  //    other gpu
  // The default is #2 above. If *both* a source and dest. are set for #1
  // above, then that overides both #2 and #3
  void set_src_pool(int pool_id) {src_pool_id_ = pool_id;}
  void set_dst_pool(int pool_id) {dst_pool_id_ = pool_id;}
  void set_full_test(bool full_test) {do_full_test_ = full_test;}
  int pool_index(void) const {return pool_index_;}
  void set_pool_index(int i) {pool_index_ = i;}
  int agent_index(void) const {return agent_index_;}
  void set_agent_index(int i) {agent_index_ = i;}
  std::vector<PoolInfo *> *pool_info(void) {return &pool_info_;}
  std::vector<AgentInfo *> *agent_info(void) {return &agent_info_;}
  std::vector<NodeInfo> *node_info(void) {return &node_info_;}

  // @Brief: Display information about what this test does
  virtual void DisplayTestInfo(void);

 private:
  // @Brief: Get real iteration number
  virtual size_t RealIterationNum(void);

  // @Brief: Get the mean copy time
  double GetMeanTime(std::vector<double>* vec);

  // @Brief: Find and print out the needed topology info
  void FindTopology(void);

  // @Brief: Run for Benchmark mode with verification
  void RunBenchmarkWithVerification(Transaction *t);

  // @Brief: Dispaly Benchmark result
  void DisplayBenchmark(Transaction *t) const;

  // @Brief: Print topology info
  void PrintTopology(void);

  void ConstructTransactionList(void);

  // @Brief: Find system region
  void FindSystemPool(void);

  // More variables declared for testing
  std::vector<Transaction> tran_;

  // Variable used to store agent info, indexed by agent_index_
  std::vector<AgentInfo *> agent_info_;

  // Variable used to store region info, indexed by pool_index_
  std::vector<PoolInfo *> pool_info_;

  // To store node info
  std::vector<NodeInfo> node_info_;

  // Variable to help count agent index
  int agent_index_;

  // Variable to help count region index
  int pool_index_;

  // Verification result
  bool verified_;

  // Store the testing level
  int src_pool_id_;
  int dst_pool_id_;
  bool do_full_test_;

  // System region
  hsa_amd_memory_pool_t sys_pool_;

  // CPU agent used for verification
  hsa_agent_t cpu_agent_;

  rocrtst::PerfTimer copy_timer_;
};

#endif  // ROCRTST_SUITES_PERFORMANCE_MEMORY_ASYNC_COPY_H_
