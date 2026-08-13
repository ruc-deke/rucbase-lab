// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "common/context.h"

#include "common/wire_result.h"

Context::Context(LockManager* lock_manager, LogManager* log_manager, Transaction* txn)
    : lock_manager_(lock_manager),
      log_manager_(log_manager),
      txn_(txn),
      result_(std::make_unique<QueryResultBuilder>()) {}

Context::~Context() = default;
Context::Context(Context&&) noexcept = default;
Context& Context::operator=(Context&&) noexcept = default;

QueryResultBuilder& Context::result() noexcept { return *result_; }

const QueryResultBuilder& Context::result() const noexcept { return *result_; }
