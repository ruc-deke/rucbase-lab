// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <gtest/gtest.h>

#include "transaction/transaction.h"
#include "transaction/transaction_manager.h"

TEST(TransactionOwnershipTest, AppendsWriteRecordByValue) {
    Transaction txn(1);
    txn.append_write_record(WriteRecord(WType::INSERT_TUPLE, "t", Rid{1, 0}));
    ASSERT_EQ(txn.write_set().size(), 1U);
    EXPECT_EQ(txn.write_set().front().table_name(), "t");
    EXPECT_EQ(txn.write_set().front().write_type(), WType::INSERT_TUPLE);
}

TEST(TransactionOwnershipTest, MissingTransactionIsNull) {
    TransactionManager manager(nullptr, nullptr);
    EXPECT_EQ(manager.get_transaction(INVALID_TXN_ID), nullptr);
    EXPECT_EQ(manager.get_transaction(1), nullptr);
}
