#include <unordered_map>
#include <memory>
#include <optional>

template <typename K, typename V>
struct Node {
    K key;
    V value;
    Node* next;
    Node* prev;  
    Node(){}
    Node(K key, V val) : key(key), value(val), next(nullptr), prev(nullptr){}
};

template <typename K, typename V>
class LRU {
public:
    LRU(size_t capacity);
    std::optional<V> get(K key);
    void put(K key, V val);
    
private:
    size_t capacity_;
    Node<K, V> dummy;
    std::unordered_map<K, std::unique_ptr<Node<K, V>>> umap_;
    void del(Node<K, V>*);
    void add_head(Node<K, V>*);
};
template <typename K, typename V>
LRU<K, V>::LRU(size_t capacity) : capacity_(capacity) {
    dummy.next = &dummy;
    dummy.prev = &dummy;
}

template <typename K, typename V>
std::optional<V> LRU<K, V>::get(K key) {
    auto it = umap_.find(key);
    if (it == umap_.end()) {
        return std::nullopt;
    }
    Node<K, V>* node = it->second.get();
    del(node);
    add_head(node);
    return it->second->value;
}

template <typename K, typename V>
void LRU<K, V>::put(K key, V val) {
    auto it = umap_.find(key);
    if (it == umap_.end()) {
        auto node = std::make_unique<Node<K, V>>(key, val);
        if (capacity_ == umap_.size()) {
            K target = dummy.prev->key;
            del(dummy.prev);
            umap_.erase(target);
        }
        add_head(node.get());
        umap_[key] = std::move(node);
        return;
    }
    it->second->value = val;
    del(it->second.get());
    add_head(it->second.get());
}

template <typename K, typename V>
void LRU<K, V>::del(Node<K, V>* node) {
    node->next->prev = node->prev;
    node->prev->next = node->next;
}

template <typename K, typename V>
void LRU<K, V>::add_head(Node<K, V>* node) {
    node->next = dummy.next;
    dummy.next->prev = node;
    dummy.next = node;
    node->prev = &dummy;
}
