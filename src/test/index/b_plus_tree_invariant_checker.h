// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <string>
#include <vector>

class BPlusTree;

/** @brief B+ tree structural validation result used by the Lab 2 tests. */
struct BPlusTreeInvariantReport {
    std::vector<std::string> violations;

    [[nodiscard]] bool ok() const noexcept { return violations.empty(); }

    /** @return A compact multi-line diagnostic suitable for a GoogleTest failure. */
    [[nodiscard]] std::string describe() const;
};

/**
 * @brief Read-only, test-only B+ tree invariant checker.
 *
 * @pre No thread may mutate the tree while check() is running.
 * @post The buffer pool pin state is unchanged.
 */
class BPlusTreeInvariantChecker {
public:
    [[nodiscard]] static BPlusTreeInvariantReport check(const BPlusTree& tree);
};
