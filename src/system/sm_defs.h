// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0
#pragma once

#include <string>

#include "common/defs.h"

/** @brief SQL DDL 中的字段定义，尚未计算其物理偏移。 */
struct ColDef {
    std::string name;  ///< 字段名。
    ColType type{};    ///< 字段类型。
    int len{};         ///< 定长记录中的字节数。
};
