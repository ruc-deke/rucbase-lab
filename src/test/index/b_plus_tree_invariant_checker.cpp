// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "b_plus_tree_invariant_checker.h"

#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "index/b_plus_tree.h"
#include "storage/buffer_pool_manager.h"
#include "storage/disk_manager.h"

namespace {

constexpr size_t kMaxViolations = 8;
using Key = std::vector<char>;

struct NodeSnapshot {
    page_id_t parent_page = INDEX_NO_PAGE;
    bool is_leaf = false;
    int size = 0;
    page_id_t previous_leaf = INDEX_NO_PAGE;
    page_id_t next_leaf = INDEX_NO_PAGE;
    std::vector<Key> keys;
    std::vector<page_id_t> children;
};

struct SubtreeSummary {
    Key minimum;
    Key maximum;
};

struct LeafSnapshot {
    page_id_t page_no = INDEX_NO_PAGE;
    page_id_t previous_leaf = INDEX_NO_PAGE;
    page_id_t next_leaf = INDEX_NO_PAGE;
};

class Checker {
public:
    Checker(const IndexFileHeader& header,
            BufferPoolManager& buffer_pool_manager,
            DiskManager& disk_manager,
            int file_descriptor)
        : header_(header),
          buffer_pool_manager_(buffer_pool_manager),
          file_descriptor_(file_descriptor),
          high_water_page_(disk_manager.get_fd2pageno(file_descriptor)) {}

    BPlusTreeInvariantReport run() {
        const auto sentinel = read_node(INDEX_LEAF_HEADER_PAGE, true, "leaf sentinel");
        if (sentinel.has_value()) check_sentinel_header(*sentinel);

        // Some student implementations keep an empty root leaf while others use
        // INDEX_NO_PAGE. Both representations are acceptable to this test helper.
        if (header_.root_page_ == INDEX_NO_PAGE) {
            if (header_.first_leaf_ != header_.last_leaf_) {
                add("empty tree has different first_leaf and last_leaf");
            } else if (header_.first_leaf_ != INDEX_LEAF_HEADER_PAGE && !valid_tree_page(header_.first_leaf_)) {
                add("empty tree leaf endpoint is outside the allocated page range");
            } else if (sentinel.has_value() &&
                       (sentinel->previous_leaf != header_.first_leaf_ || sentinel->next_leaf != header_.first_leaf_)) {
                add("empty tree leaf sentinel does not point to the header leaf");
            }
            return std::move(report_);
        }

        (void)visit(header_.root_page_, INDEX_NO_PAGE, 0, "root");
        check_leaf_chain(sentinel);

        const int reachable_page_count = static_cast<int>(visited_pages_.size()) + 2;
        if (header_.page_count_ != reachable_page_count) {
            add("page_count=" + std::to_string(header_.page_count_) + ", but " + std::to_string(reachable_page_count) +
                " pages are reachable (including reserved pages)");
        }
        return std::move(report_);
    }

private:
    void check_sentinel_header(const NodeSnapshot& sentinel) {
        if (!sentinel.is_leaf) add("leaf sentinel page is not marked as a leaf");
        if (sentinel.size != 0) add("leaf sentinel has size " + std::to_string(sentinel.size) + ", expected 0");
        if (sentinel.parent_page != INDEX_NO_PAGE) {
            add("leaf sentinel parent is " + std::to_string(sentinel.parent_page) + ", expected INDEX_NO_PAGE");
        }
    }

    std::optional<NodeSnapshot> read_node(page_id_t page_no, bool allow_sentinel, const std::string& context) {
        const page_id_t minimum = allow_sentinel ? INDEX_LEAF_HEADER_PAGE : INDEX_INITIAL_ROOT_PAGE;
        if (page_no < minimum || page_no >= high_water_page_) {
            add(context + ": page " + std::to_string(page_no) + " is outside [" + std::to_string(minimum) + ", " +
                std::to_string(high_water_page_) + ")");
            return std::nullopt;
        }

        auto guard = buffer_pool_manager_.fetch_page_guard(PageId{.fd = file_descriptor_, .page_no = page_no});
        if (!guard) {
            add(context + ": failed to fetch page " + std::to_string(page_no));
            return std::nullopt;
        }

        BPlusTreeNode node(&header_, guard.get());
        NodeSnapshot snapshot{.parent_page = node.get_parent_page_no(),
                              .is_leaf = node.is_leaf_page(),
                              .size = node.get_size(),
                              .previous_leaf = node.get_prev_leaf(),
                              .next_leaf = node.get_next_leaf()};
        if (snapshot.size < 0 || snapshot.size > header_.tree_order_ + 1) {
            add(context + ": page " + std::to_string(page_no) + " has invalid size " + std::to_string(snapshot.size));
            return std::nullopt;
        }

        snapshot.keys.reserve(static_cast<size_t>(snapshot.size));
        if (!snapshot.is_leaf) snapshot.children.reserve(static_cast<size_t>(snapshot.size));
        for (int i = 0; i < snapshot.size; ++i) {
            const char* key = node.get_key(i);
            snapshot.keys.emplace_back(key, key + header_.key_length_);
            if (!snapshot.is_leaf) snapshot.children.push_back(node.value_at(i));
        }
        return snapshot;
    }

    std::optional<SubtreeSummary> visit(page_id_t page_no,
                                        page_id_t expected_parent,
                                        int depth,
                                        const std::string& path) {
        if (active_pages_.contains(page_no)) {
            add(path + ": child graph contains a cycle through page " + std::to_string(page_no));
            return std::nullopt;
        }
        if (visited_pages_.contains(page_no)) {
            add(path + ": page " + std::to_string(page_no) + " is reachable more than once");
            return std::nullopt;
        }
        if (!valid_tree_page(page_no)) {
            add(path + ": child page " + std::to_string(page_no) + " is outside the allocated index-page range");
            return std::nullopt;
        }

        visited_pages_.insert(page_no);
        active_pages_.insert(page_no);
        const auto snapshot = read_node(page_no, false, path);
        if (!snapshot.has_value()) {
            active_pages_.erase(page_no);
            return std::nullopt;
        }
        if (snapshot->parent_page != expected_parent) {
            add(path + ": page " + std::to_string(page_no) + " parent=" + std::to_string(snapshot->parent_page) +
                ", expected " + std::to_string(expected_parent));
        }

        const int maximum_size = header_.tree_order_ + 1;
        if (snapshot->size >= maximum_size) {
            add(path + ": page " + std::to_string(page_no) + " size=" + std::to_string(snapshot->size) +
                " must be less than max_size=" + std::to_string(maximum_size));
        }
        const bool is_root = page_no == header_.root_page_;
        const int minimum_size = maximum_size / 2;
        if (!is_root && snapshot->size < minimum_size) {
            add(path + ": non-root page " + std::to_string(page_no) + " size=" + std::to_string(snapshot->size) +
                " is below min_size=" + std::to_string(minimum_size));
        }
        if (is_root && !snapshot->is_leaf && snapshot->size < 2) {
            add(path + ": internal root has fewer than two children and should have been collapsed");
        }

        check_local_key_order(*snapshot, path);

        std::optional<SubtreeSummary> result;
        if (snapshot->is_leaf) {
            if (!leaf_depth_.has_value()) {
                leaf_depth_ = depth;
            } else if (*leaf_depth_ != depth) {
                add(path + ": leaf page " + std::to_string(page_no) + " is at depth " + std::to_string(depth) +
                    ", expected " + std::to_string(*leaf_depth_));
            }
            if (!is_root && snapshot->size == 0) {
                add(path + ": a non-root leaf is empty");
            }

            leaves_.push_back(
                {.page_no = page_no, .previous_leaf = snapshot->previous_leaf, .next_leaf = snapshot->next_leaf});
            if (!snapshot->keys.empty()) {
                result = SubtreeSummary{.minimum = snapshot->keys.front(), .maximum = snapshot->keys.back()};
            }
        } else {
            std::vector<std::optional<SubtreeSummary>> child_summaries;
            child_summaries.reserve(snapshot->children.size());
            for (size_t i = 0; i < snapshot->children.size(); ++i) {
                child_summaries.push_back(
                    visit(snapshot->children[i], page_no, depth + 1, path + "/child[" + std::to_string(i) + "]"));
                if (child_summaries.back().has_value()) {
                    if (compare_keys(snapshot->keys[i], child_summaries.back()->minimum) != 0) {
                        add(path + ": key[" + std::to_string(i) + "] does not equal child " +
                            std::to_string(snapshot->children[i]) + " subtree minimum");
                    }
                }
            }

            for (size_t i = 1; i < child_summaries.size(); ++i) {
                if (!child_summaries[i - 1].has_value() || !child_summaries[i].has_value()) continue;
                if (compare_keys(child_summaries[i - 1]->maximum, child_summaries[i]->minimum) >= 0) {
                    add(path + ": child " + std::to_string(snapshot->children[i - 1]) +
                        " subtree overlaps or is out of order with child " + std::to_string(snapshot->children[i]));
                }
            }

            if (!child_summaries.empty() && child_summaries.front().has_value() && child_summaries.back().has_value()) {
                result = SubtreeSummary{.minimum = child_summaries.front()->minimum,
                                        .maximum = child_summaries.back()->maximum};
            }
        }

        active_pages_.erase(page_no);
        return result;
    }

    void check_local_key_order(const NodeSnapshot& node, const std::string& path) {
        for (size_t i = 1; i < node.keys.size(); ++i) {
            if (compare_keys(node.keys[i - 1], node.keys[i]) >= 0) {
                add(path + ": keys at slots " + std::to_string(i - 1) + " and " + std::to_string(i) +
                    " are not strictly increasing");
            }
        }
    }

    void check_leaf_chain(const std::optional<NodeSnapshot>& sentinel) {
        if (leaves_.empty()) {
            add("reachable tree contains no leaf pages");
            return;
        }

        if (header_.first_leaf_ != leaves_.front().page_no) {
            add("file header first_leaf=" + std::to_string(header_.first_leaf_) +
                ", DFS first leaf=" + std::to_string(leaves_.front().page_no));
        }
        if (header_.last_leaf_ != leaves_.back().page_no) {
            add("file header last_leaf=" + std::to_string(header_.last_leaf_) +
                ", DFS last leaf=" + std::to_string(leaves_.back().page_no));
        }
        if (sentinel.has_value()) {
            if (sentinel->next_leaf != leaves_.front().page_no) {
                add("leaf sentinel next=" + std::to_string(sentinel->next_leaf) + ", expected " +
                    std::to_string(leaves_.front().page_no));
            }
            if (sentinel->previous_leaf != leaves_.back().page_no) {
                add("leaf sentinel previous=" + std::to_string(sentinel->previous_leaf) + ", expected " +
                    std::to_string(leaves_.back().page_no));
            }
        }

        for (size_t i = 0; i < leaves_.size(); ++i) {
            const page_id_t expected_previous = i == 0 ? INDEX_LEAF_HEADER_PAGE : leaves_[i - 1].page_no;
            const page_id_t expected_next = i + 1 == leaves_.size() ? INDEX_LEAF_HEADER_PAGE : leaves_[i + 1].page_no;
            if (leaves_[i].previous_leaf != expected_previous) {
                add("leaf page " + std::to_string(leaves_[i].page_no) + " previous=" +
                    std::to_string(leaves_[i].previous_leaf) + ", expected " + std::to_string(expected_previous));
            }
            if (leaves_[i].next_leaf != expected_next) {
                add("leaf page " + std::to_string(leaves_[i].page_no) +
                    " next=" + std::to_string(leaves_[i].next_leaf) + ", expected " + std::to_string(expected_next));
            }
        }
    }

    int compare_keys(const Key& left, const Key& right) const {
        return compare_index_key(left.data(), right.data(), header_.column_types_, header_.column_lengths_);
    }

    bool valid_tree_page(page_id_t page_no) const noexcept {
        return page_no >= INDEX_INITIAL_ROOT_PAGE && page_no < high_water_page_;
    }

    void add(std::string violation) {
        if (report_.violations.size() < kMaxViolations) report_.violations.push_back(std::move(violation));
    }

    const IndexFileHeader& header_;
    BufferPoolManager& buffer_pool_manager_;
    int file_descriptor_ = -1;
    page_id_t high_water_page_ = 0;
    BPlusTreeInvariantReport report_;
    std::unordered_set<page_id_t> visited_pages_;
    std::unordered_set<page_id_t> active_pages_;
    std::vector<LeafSnapshot> leaves_;
    std::optional<int> leaf_depth_;
};

}  // namespace

std::string BPlusTreeInvariantReport::describe() const {
    std::ostringstream out;
    out << (ok() ? "B+ tree invariants hold" : "B+ tree invariant violations:");
    for (const auto& violation : violations) out << "\n  - " << violation;
    return out.str();
}

BPlusTreeInvariantReport BPlusTreeInvariantChecker::check(const BPlusTree& tree) {
    return Checker(*tree.file_header_, *tree.buffer_pool_manager_, *tree.disk_manager_, tree.file_descriptor_).run();
}
