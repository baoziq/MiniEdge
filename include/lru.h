#include <unordered_map>
#include <memory>

struct Node {
    int key;
    int value;
    Node* next;
    Node* prev;  
    Node(){}
    Node(int key, int val) : key(key), value(val), next(nullptr), prev(nullptr){}
};

class LRU {
public:
    LRU(size_t capacity);
    int get(int key);
    void put(int key, int val);
    
private:
    size_t capacity_;
    Node dummy;
    std::unordered_map<int, std::unique_ptr<Node>> umap_;
    void del(Node*);
    void add_head(Node*);
};

