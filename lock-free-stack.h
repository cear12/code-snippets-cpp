#include <atomic>
#include <memory>
#include <thread>
#include <array>

template<typename T>
class LockFreeStack {
private:
    struct Node {
        std::atomic<T*> data;
        std::atomic<Node*> next;
        
        Node() : data(nullptr), next(nullptr) {}
    };
    
    std::atomic<Node*> head;
    
    // Hazard Pointer для защиты от ABA проблемы
    static constexpr int MAX_THREADS = 100;
    struct HazardPointer {
        std::atomic<std::thread::id> id;
        std::atomic<Node*> pointer;
    };
    
    static std::array<HazardPointer, MAX_THREADS> hazard_pointers;
    static std::atomic<Node*> to_be_deleted;
    
    static HazardPointer* get_hazard_pointer_for_current_thread() {
        thread_local static HazardPointer* hp = nullptr;
        
        if (!hp) {
            auto thread_id = std::this_thread::get_id();
            for (auto& hazard : hazard_pointers) {
                std::thread::id default_id;
                if (hazard.id.compare_exchange_strong(default_id, thread_id)) {
                    hp = &hazard;
                    break;
                }
            }
        }
        return hp;
    }
    
    static bool is_pointer_hazardous(Node* ptr) {
        for (const auto& hazard : hazard_pointers) {
            if (hazard.pointer.load() == ptr) {
                return true;
            }
        }
        return false;
    }
    
    static void delete_nodes_no_hazards() {
        Node* current = to_be_deleted.exchange(nullptr);
        
        while (current) {
            Node* next = current->next.load();
            
            if (!is_pointer_hazardous(current)) {
                delete current->data.load();
                delete current;
            } else {
                // Возвращаем в список на удаление
                current->next.store(to_be_deleted.load());
                while (!to_be_deleted.compare_exchange_weak(current->next.load(), current));
            }
            
            current = next;
        }
    }
    
public:
    LockFreeStack() : head(nullptr) {}
    
    ~LockFreeStack() {
        while (Node* old_head = head.load()) {
            head.store(old_head->next.load());
            delete old_head->data.load();
            delete old_head;
        }
        
        // Очищаем список отложенного удаления
        delete_nodes_no_hazards();
    }
    
    void push(T item) {
        Node* new_node = new Node;
        T* data = new T(std::move(item));
        new_node->data.store(data);
        
        Node* current_head = head.load();
        do {
            new_node->next.store(current_head);
        } while (!head.compare_exchange_weak(current_head, new_node));
    }
    
    std::unique_ptr<T> pop() {
        HazardPointer* hp = get_hazard_pointer_for_current_thread();
        if (!hp) {
            return nullptr; // Нет доступных hazard pointers
        }
        
        Node* old_head = head.load();
        
        do {
            Node* temp = old_head;
            hp->pointer.store(old_head);
            
            // Перепроверяем, что head не изменился
            old_head = head.load();
            if (old_head != temp) {
                continue;
            }
            
            if (!old_head) {
                hp->pointer.store(nullptr);
                return nullptr;
            }
            
        } while (!head.compare_exchange_weak(old_head, old_head->next.load()));
        
        hp->pointer.store(nullptr);
        
        std::unique_ptr<T> result;
        if (old_head->data.load()) {
            result = std::make_unique<T>(*old_head->data.load());
        }
        
        // Отложенное удаление
        old_head->next.store(to_be_deleted.load());
        while (!to_be_deleted.compare_exchange_weak(old_head->next.load(), old_head));
        
        // Периодически пытаемся удалить безопасные узлы
        if (std::this_thread::get_id() == std::hash<std::thread::id>{}(std::this_thread::get_id()) % 10 == 0) {
            delete_nodes_no_hazards();
        }
        
        return result;
    }
    
    bool empty() const {
        return head.load() == nullptr;
    }
};

// Статические члены
template<typename T>
std::array<typename LockFreeStack<T>::HazardPointer, LockFreeStack<T>::MAX_THREADS> 
    LockFreeStack<T>::hazard_pointers = {};

template<typename T>
std::atomic<typename LockFreeStack<T>::Node*> LockFreeStack<T>::to_be_deleted{nullptr};
