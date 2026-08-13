// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "system/sm_meta.h"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "common/context.h"
#include "common/wire_result.h"
#include "system/sm_manager.h"

TEST(CatalogMetadataTest, WritesReadableEmptyDatabaseJson) {
    std::istringstream input(R"({"database":"demo","tables":[]})");
    DbMeta db;
    input >> db;

    std::ostringstream output;
    output << db;
    EXPECT_EQ(output.str(), "{\n  \"database\": \"demo\",\n  \"tables\": []\n}\n");
}

TEST(CatalogMetadataTest, ShowDatabaseReturnsStructuredCurrentName) {
    SmManager sm_manager(nullptr, nullptr, nullptr, nullptr);
    std::istringstream input(R"({"database":"demo","tables":[]})");
    input >> sm_manager.db_;

    Context context(nullptr, nullptr, nullptr);
    sm_manager.show_database(&context);

    const auto& result = context.result().view();
    ASSERT_TRUE(result.has_query_result);
    ASSERT_EQ(result.columns.size(), 1U);
    EXPECT_EQ(result.columns[0].name, "Database");
    EXPECT_EQ(result.columns[0].type, TYPE_STRING);
    ASSERT_EQ(result.rows.size(), 1U);
    ASSERT_EQ(result.rows[0].size(), 1U);
    EXPECT_EQ(result.rows[0][0].str_val, "demo");
}

TEST(CatalogMetadataTest, DefaultMetadataIsInitialized) {
    ColMeta column;
    IndexMeta index;
    EXPECT_EQ(column.len, 0);
    EXPECT_EQ(column.offset, 0);
    EXPECT_FALSE(column.index);
    EXPECT_EQ(index.col_num, 0);
    EXPECT_EQ(index.col_tot_len, 0);
}

TEST(CatalogMetadataTest, RebuildsDerivedMetadataOnRoundTrip) {
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
    EXPECT_FALSE(table.indexes[0].unique);
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

    EXPECT_NE(output.str().find("\"unique\": false"), std::string::npos);

    std::istringstream replacement(R"({"database":"empty","tables":[]})");
    replacement >> restored;
    EXPECT_FALSE(restored.is_table("student"));
}

TEST(CatalogMetadataTest, ReadsUniqueIndexFlag) {
    const std::string json = R"({
        "database": "demo",
        "tables": [{
          "name": "t",
          "columns": [{"name": "id", "type": "INT", "length": 4}],
          "indexes": [{"columns": ["id"], "unique": true}]
        }]
      })";
    DbMeta db;
    std::istringstream input(json);
    input >> db;
    ASSERT_TRUE(db.get_table("t").indexes[0].unique);

    std::ostringstream output;
    output << db;
    EXPECT_NE(output.str().find("\"unique\": true"), std::string::npos);
}

TEST(CatalogMetadataTest, RejectsLegacyAndInvalidMetadata) {
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

TEST(CatalogMetadataTest, PreservesJsonEscapes) {
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

TEST(CatalogMetadataTest, AcceptsReorderedObjectFields) {
    std::istringstream input(
        R"({"tables":[{"indexes":[],"columns":[{"length":4,"type":"INT","name":"id"}],"name":"t"}],"database":"demo"})");

    DbMeta db;
    EXPECT_NO_THROW(input >> db);
    ASSERT_TRUE(db.is_table("t"));
    const DbMeta& catalog = db;
    const TabMeta& table = catalog.get_table("t");
    EXPECT_EQ(table.get_col("id")->name, "id");
}
