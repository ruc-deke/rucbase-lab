// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <iosfwd>
#include <string>

#include "net/client.h"

namespace rucbase::test {

/**
 * @brief Execute one SQL statement and emit its observable result as one JSONL event.
 *
 * Result sets are consumed through the typed Wire callbacks with text formatting disabled.
 * Test fixtures use valid UTF-8 for CHAR values and column names.
 * Successful commands emit no event. SQL errors and transaction aborts emit a status event;
 * transport and protocol failures are reported to diagnostics and return false.
 *
 * @param client Connected Wire client.
 * @param sql One SQL statement, or a blank/comment line to ignore.
 * @param output Destination for a complete JSON object followed by a newline.
 * @param diagnostics Destination for infrastructure-error diagnostics.
 * @return true when the statement was ignored or produced a complete protocol response.
 */
bool ExecuteAndWriteTypedResult(wire::Client* client,
                                const std::string& sql,
                                std::ostream& output,
                                std::ostream& diagnostics);

/**
 * @brief Execute SQL without retaining formatted text.
 *
 * This is used for concurrency-test preload commands whose successful results are intentionally
 * not part of the golden output.
 */
wire::ExecuteResult ExecuteWithoutText(wire::Client* client, const std::string& sql);

}  // namespace rucbase::test
