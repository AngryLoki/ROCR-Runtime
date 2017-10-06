#include "common.hpp"
#include "rocm_async.hpp"

bool RocmAsync::BuildReadOrWriteTrans(uint32_t req_type,
                                      vector<uint32_t>& in_list) {
  
  // Validate the list of pool-agent tuples
  hsa_status_t status;
  hsa_amd_memory_pool_access_t access;
  uint32_t list_size = in_list.size();
  for (uint32_t idx = 0; idx < list_size; idx+=2) {
    
    uint32_t pool_idx = in_list[idx];
    uint32_t exec_idx = in_list[idx + 1];
    
    // Retrieve Roc runtime handles for memory pool and agent
    hsa_agent_t exec_agent = agent_list_[exec_idx].agent_;
    hsa_amd_memory_pool_t pool = pool_list_[pool_idx].pool_;
  
    // Determine agent can access the memory pool
    status = hsa_amd_agent_memory_pool_get_info(exec_agent, pool,
                           HSA_AMD_AGENT_MEMORY_POOL_INFO_ACCESS, &access);
    ErrorCheck(status);
    
    // Determine if accessibility to agent is not denied
    if (access == HSA_AMD_MEMORY_POOL_ACCESS_NEVER_ALLOWED) {
      PrintIOAccessError(exec_idx, pool_idx);
      return false;
    }

    // Agent has access, build an instance of transaction
    // and add it to the list of transactions
    async_trans_t trans(req_type);
    trans.kernel.code_ = nullptr;
    trans.kernel.pool_ = pool;
    trans.kernel.pool_idx_ = pool_idx;
    trans.kernel.agent_ = exec_agent;
    trans.kernel.agent_idx_ = exec_idx;
    trans_list_.push_back(trans);
  }
  return true;
}

bool RocmAsync::BuildReadTrans() {
  return BuildReadOrWriteTrans(REQ_READ, read_list_);
}

bool RocmAsync::BuildWriteTrans() {
  return BuildReadOrWriteTrans(REQ_WRITE, write_list_);
}

bool RocmAsync::BuildCopyTrans(uint32_t req_type,
                               vector<uint32_t>& src_list,
                               vector<uint32_t>& dst_list) {

  uint32_t src_size = src_list.size();
  uint32_t dst_size = dst_list.size();
  
  hsa_status_t status;
  hsa_amd_memory_pool_access_t access;
  for (uint32_t idx = 0; idx < src_size; idx++) {
    
    // Retrieve Roc runtime handles for Src memory pool and agents
    uint32_t src_idx = src_list[idx];
    hsa_agent_t src_agent = pool_list_[src_idx].owner_agent_;
    hsa_amd_memory_pool_t src_pool = pool_list_[src_idx].pool_;
    uint32_t src_dev_idx = pool_list_[src_idx].agent_index_;
    hsa_device_type_t src_dev_type = agent_list_[src_dev_idx].device_type_;

    for (uint32_t jdx = 0; jdx < dst_size; jdx++) {
    
      // Retrieve Roc runtime handles for Dst memory pool and agents
      uint32_t dst_idx = dst_list[jdx];
      hsa_agent_t dst_agent = pool_list_[dst_idx].owner_agent_;
      hsa_amd_memory_pool_t dst_pool = pool_list_[dst_idx].pool_;
      uint32_t dst_dev_idx = pool_list_[dst_idx].agent_index_;
      hsa_device_type_t dst_dev_type = agent_list_[dst_dev_idx].device_type_;

      // Filter out transaction when Src & Dst pools belong to Cpu
      if ((src_dev_type == HSA_DEVICE_TYPE_CPU) &&
          (dst_dev_type == HSA_DEVICE_TYPE_CPU)) {
        continue;
      }

      // Filter out transaction with same Src & Dst pools
      if (src_idx == dst_idx) {
        continue;
      }
      
      // Determine if accessibility to src pool for dst agent is not denied
      status = hsa_amd_agent_memory_pool_get_info(dst_agent, src_pool,
                             HSA_AMD_AGENT_MEMORY_POOL_INFO_ACCESS, &access);
      ErrorCheck(status);
      if (access == HSA_AMD_MEMORY_POOL_ACCESS_NEVER_ALLOWED) {
        PrintCopyAccessError(src_idx, dst_idx);
        return false;
      }

      // Determine if accessibility to dst pool for src agent is not denied
      status = hsa_amd_agent_memory_pool_get_info(src_agent, dst_pool,
                             HSA_AMD_AGENT_MEMORY_POOL_INFO_ACCESS, &access);
      ErrorCheck(status);
      if (access == HSA_AMD_MEMORY_POOL_ACCESS_NEVER_ALLOWED) {
        return false;
      }

      // Agents have access, build an instance of transaction
      // and add it to the list of transactions
      async_trans_t trans(req_type);
      trans.copy.src_idx_ = src_idx;
      trans.copy.dst_idx_ = dst_idx;
      trans.copy.src_pool_ = src_pool;
      trans.copy.dst_pool_ = dst_pool;
      trans.copy.bidir_ = (req_type == REQ_COPY_BIDIR);
      trans.copy.uses_gpu_ = ((src_dev_type == HSA_DEVICE_TYPE_GPU) ||
                              (dst_dev_type == HSA_DEVICE_TYPE_GPU));
      trans_list_.push_back(trans);
    }
  }
  return true;
}

bool RocmAsync::BuildBidirCopyTrans() {
  return BuildCopyTrans(REQ_COPY_BIDIR, bidir_list_, bidir_list_);
}

bool RocmAsync::BuildUnidirCopyTrans() {
  return BuildCopyTrans(REQ_COPY_UNIDIR, src_list_, dst_list_);
}

// @brief: Builds a list of transaction per user request
bool RocmAsync::BuildTransList() {
  
  // Build list of Read transactions per user request
  bool status = false;
  if (req_read_ == REQ_READ) {
    status = BuildReadTrans();
    if (status == false) {
      return status;
    }
  }

  // Build list of Write transactions per user request
  status = false;
  if (req_write_ == REQ_WRITE) {
    status = BuildWriteTrans();
    if (status == false) {
      return status;
    }
  }

  // Build list of Bidirectional Copy transactions per user request
  status = false;
  if (req_copy_bidir_ == REQ_COPY_BIDIR) {
    status = BuildBidirCopyTrans();
    if (status == false) {
      return status;
    }
  }

  // Build list of Unidirectional Copy transactions per user request
  status = false;
  if (req_copy_unidir_ == REQ_COPY_UNIDIR) {
    status = BuildUnidirCopyTrans();
    if (status == false) {
      return status;
    }
  }

  // All of the transaction are built up
  return true;
}
