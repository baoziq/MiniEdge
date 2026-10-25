#include "lru.h"
#include <memory>

LRU::LRU(size_t capacity) : capacity_(capacity) {
    dummy.next = &dummy;
    dummy.prev = &dummy;
}

int LRU::get(int key) {
    auto it = umap_.find(key);
    if (it == umap_.end()) {
        return -1;
    }
    Node* node = it->second.get();
    del(node);
    add_head(node);
    return it->second->value;
}

void LRU::put(int key, int val) {
    auto it = umap_.find(key);
    if (it == umap_.end()) {
        auto node = std::make_unique<Node>(key, val);
        if (capacity_ == umap_.size()) {
            del(dummy.prev);
            umap_.erase(key);
        }
        add_head(node.get());
        umap_[key] = std::move(node);
        return;
    }
    it->second->value = val;
    del(it->second.get());
    add_head(it->second.get());
}

void LRU::del(Node* node) {
    node->next->prev = node->prev;
    node->prev->next = node->next;
}

void LRU::add_head(Node* node) {
    node->next = dummy.next;
    dummy.next->prev = node;
    dummy.next = node;
    node->prev = &dummy;
}