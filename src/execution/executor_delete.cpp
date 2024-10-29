#include "executor_delete.h"

std::unique_ptr<RmRecord> DeleteExecutor::Next() {
 // !djb20241013 Todo:
        // 1. 遍历所有需要删除的record的rid
        // 2. 将指定rid的record通过RmFileHandle的delete_record函数从表数据文件中删除
        // 3. 如果表上存在索引，
        // 4. 将指定rid的record通过IxIndexHandle的delete_entry函数从索引文件中删除
        // lab4: 记录删除操作（for transaction rollback）

        // insert和delete操作不需要返回record对应指针，返回nullptr即可
    RmRecord rec(fh_->get_file_hdr().record_size);

        // 1. 将record对象通过RmFileHandle插入到对应表的数据文件中
    for (const auto &rid : rids_) {
        fh_->delete_record(rid, context_);
    }

        // 2. 如果表上存在索引，将record对象插入到相关索引文件中
    for (size_t i = 0; i < tab_.indexes.size(); ++i) {
        auto &index = tab_.indexes[i];
        auto ih = sm_manager_->ihs_.at(sm_manager_->get_ix_manager()->get_index_name(tab_name_, index.cols)).get();
        char *key = new char[index.col_tot_len];
        int offset = 0;
        for (size_t i = 0; i < index.col_num; ++i) {
            memcpy(key + offset, rec.data + index.cols[i].offset, index.cols[i].len);
            offset += index.cols[i].len;
        }
        ih->delete_entry(key, context_->txn_);
    }

        // lab4: 记录插入操作（for transaction rollback）
    for (const auto &rid : rids_) {
        WriteRecord *write_rec = new WriteRecord(WType::DELETE_TUPLE, tab_name_, rid);
        context_->txn_->append_write_record(write_rec);
    }
        // insert和delete操作不需要返回record对应指针，返回nullptr即可
    return nullptr;
}

Rid & DeleteExecutor::rid() {
	return _abstract_rid;
}


