// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "log_recovery.h"

/**
 * @description: analyze阶段，需要获得脏页表（DPT）和未完成的事务列表（ATT）
 */
void RecoveryManager::analyze() {}

/**
 * @description: 重做所有未落盘的操作
 */
void RecoveryManager::redo() {}

/**
 * @description: 回滚未完成的事务
 */
void RecoveryManager::undo() {}