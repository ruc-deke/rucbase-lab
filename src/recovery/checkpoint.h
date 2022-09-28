#pragma once

#include "transaction/transaction_manager.h"

class CheckpointManager {
   public:
    CheckpointManager(TransactionManager * txn_manager, LogManager *log_manager,
                      BufferPoolManager *buffer_pool_manager)
        : txn_manager_(txn_manager), log_manager_(log_manager),
          buffer_pool_manager_(buffer_pool_manager){}

    ~CheckpointManager() = default;

    void BeginCheckpoint();
    void EndCheckpoint();

   private:
    TransactionManager *txn_manager_;
    LogManager *log_manager_;
    BufferPoolManager *buffer_pool_manager_;
};