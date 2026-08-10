// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "system/sm_meta.h"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

TEST(SmMetaTest, WritesReadableEmptyDatabaseJson) {
    std::istringstream input(R"({"database":"demo","tables":[]})");
    DbMeta db;
    input >> db;

    std::ostringstream output;
    output << db;
    EXPECT_EQ(output.str(), "{\n  \"database\": \"demo\",\n  \"tables\": []\n}\n");
}

TEST(SmMetaTest, DefaultMetadataIsInitialized) {
    ColMeta column;
    IndexMeta index;
    EXPECT_EQ(column.len, 0);
    EXPECT_EQ(column.offset, 0);
    EXPECT_FALSE(column.index);
    EXPECT_EQ(index.col_num, 0);
    EXPECT_EQ(index.col_tot_len, 0);
}

TEST(SmMetaTest, RebuildsDerivedMetadataOnRoundTrip) {
    const std::string json = R"({
        "database": "teach\"db",
        "tables": [
          {
            "name": "student",
            "columns": [
              {"name": "id", "type": "INT", "length": 4},
              {"name": "score", "type": "FLOAT", "length": 4},
              {"name": "name", "type": "STRING", "length": 8}
            ],
            "indexes": [{"columns": ["name", "id"]}]
          }
        ]
      })";

    DbMeta db;
    std::istringstream input(json);
    input >> db;

    ASSERT_TRUE(db.is_table("student"));
    TabMeta& table = db.get_table("student");
    ASSERT_EQ(table.cols.size(), 3U);
    EXPECT_EQ(table.cols[0].offset, 0);
    EXPECT_EQ(table.cols[1].offset, 4);
    EXPECT_EQ(table.cols[2].offset, 8);
    EXPECT_TRUE(table.cols[0].index);
    EXPECT_FALSE(table.cols[1].index);
    EXPECT_TRUE(table.cols[2].index);

    ASSERT_EQ(table.indexes.size(), 1U);
    EXPECT_EQ(table.indexes[0].col_num, 2);
    EXPECT_EQ(table.indexes[0].col_tot_len, 12);
    EXPECT_EQ(table.indexes[0].cols[0].name, "name");
    EXPECT_EQ(table.indexes[0].cols[1].name, "id");

    table.indexes[0].col_num = 99;  // 查询以实际列集合为准，不依赖冗余缓存。
    EXPECT_TRUE(table.is_index({"name", "id"}));

    std::ostringstream output;
    output << db;
    EXPECT_NE(output.str().find("\"database\": \"teach\\\"db\""), std::string::npos);

    DbMeta restored;
    std::istringstream round_trip(output.str());
    round_trip >> restored;
    EXPECT_TRUE(restored.is_table("student"));

    std::istringstream replacement(R"({"database":"empty","tables":[]})");
    replacement >> restored;
    EXPECT_FALSE(restored.is_table("student"));
}

TEST(SmMetaTest, RejectsLegacyAndInvalidMetadata) {
    DbMeta db;
    std::istringstream legacy("demo\n0\n");
    EXPECT_THROW(legacy >> db, InternalError);

    std::istringstream bad_index(
        R"({"database":"demo","tables":[{"name":"t","columns":[{"name":"id","type":"INT","length":4}],"indexes":[{"columns":["missing"]}]}]})");
    EXPECT_THROW(bad_index >> db, RMDBError);

    std::istringstream bad_escape(R"({"database":"bad\x","tables":[]})");
    EXPECT_THROW(bad_escape >> db, InternalError);

    std::istringstream empty_table(R"({"database":"demo","tables":[{"name":"t","columns":[],"indexes":[]}]})");
    EXPECT_THROW(empty_table >> db, InternalError);

    std::istringstream oversized(
        R"({"database":"demo","tables":[{"name":"t","columns":[{"name":"s","type":"STRING","length":513}],"indexes":[]}]})");
    EXPECT_THROW(oversized >> db, InternalError);
}

TEST(SmMetaTest, PreservesJsonEscapes) {
    DbMeta db;
    std::istringstream input(R"({"database":"line\n\u4eba","tables":[]})");
    input >> db;

    std::ostringstream output;
    output << db;
    EXPECT_NE(output.str().find("line\\n人"), std::string::npos);

    DbMeta restored;
    std::istringstream round_trip(output.str());
    EXPECT_NO_THROW(round_trip >> restored);
}

TEST(SmMetaTest, AcceptsReorderedObjectFields) {
    std::istringstream input(
        R"({"tables":[{"indexes":[],"columns":[{"length":4,"type":"INT","name":"id"}],"name":"t"}],"database":"demo"})");

    DbMeta db;
    EXPECT_NO_THROW(input >> db);
    ASSERT_TRUE(db.is_table("t"));
    const DbMeta& catalog = db;
    const TabMeta& table = catalog.get_table("t");
    EXPECT_EQ(table.get_col("id")->name, "id");
}
