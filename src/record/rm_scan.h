#pragma once

#include "rm_defs.h"

class RmFileHandle;

class RmScan : public RecScan {
    const RmFileHandle *file_handle_;
    Rid rid_;
public:
    RmScan(const RmFileHandle *file_handle);

    void next() override;

    bool is_end() const override;

    Rid rid() const override;
};
