// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include "rm_defs.h"

class RmFileHandle;

class RmScan : public RecScan {
    const RmFileHandle *file_handle_;
    Rid rid_{.page_no = INVALID_PAGE_ID, .slot_no = -1};
public:
    explicit RmScan(const RmFileHandle *file_handle);

    void next() override;

    bool is_end() const override;

    Rid rid() const override;
};
