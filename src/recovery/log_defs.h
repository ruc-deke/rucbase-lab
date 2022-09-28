#pragma once

#include "defs.h"
#include "storage/disk_manager.h"
#include "common/config.h"

#include <atomic>
#include <chrono>

static constexpr std::chrono::duration<int64_t> FLUSH_TIMEOUT = std::chrono::seconds(1);
