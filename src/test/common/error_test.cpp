// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "common/errors.h"

TEST(ErrorTypeTest, IndexNotFoundPreservesTypeAndMessage) {
    try {
        throw IndexNotFoundError("t", std::vector<std::string>{"a", "b"});
    } catch (const RMDBError& error) {
        EXPECT_NE(dynamic_cast<const IndexNotFoundError*>(&error), nullptr);
        const std::string message = error.what();
        EXPECT_NE(message.find("t.(a, b)"), std::string::npos);
        EXPECT_EQ(message.find("Error: "), 0U);
    }
}

TEST(ErrorTypeTest, IndexExistsBuildsCompleteMessageInConstructor) {
    const IndexExistsError error("orders", std::vector<std::string>{"id"});
    const std::string message = error.what();
    EXPECT_NE(message.find("orders.(id)"), std::string::npos);
    EXPECT_EQ(message.find("Error: "), 0U);
}
