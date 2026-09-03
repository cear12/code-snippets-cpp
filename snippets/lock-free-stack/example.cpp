// Lock-free stack idiom: a Treiber stack (CAS-loop push/pop on a singly
// linked list) with hazard pointers protecting readers from a concurrent
// pop-and-delete use-after-free. See README.md for the three real bugs
// found and fixed here (a type error, an invalid reference binding used
// twice, and a do-while `continue` targeting the wrong place).
#include <array>
#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

template <typename T>
class LockFreeStack {
private:
    struct Node {
        std::atomic<T*> data;
        std::atomic<Node*> next;

        Node() : data(nullptr), next(nullptr) {}
    };

    std::atomic<Node*> head;

    // Hazard pointers: protect against the ABA / use-after-free problem
    // that comes from freeing a node while another thread might still be
    // about to dereference it.
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

    // Pushes `node` onto the to_be_deleted retire list.
    static void retire(Node* node) {
        Node* old_retired_head = to_be_deleted.load();
        do {
            node->next.store(old_retired_head);
        } while (!to_be_deleted.compare_exchange_weak(old_retired_head, node));
    }

    static void delete_nodes_no_hazards() {
        Node* current = to_be_deleted.exchange(nullptr);

        while (current) {
            Node* next = current->next.load();

            if (!is_pointer_hazardous(current)) {
                delete current->data.load();
                delete current;
            } else {
                // Still protected by someone's hazard pointer: put it
                // back on the retire list for a later pass.
                retire(current);
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

        delete_nodes_no_hazards();
    }

    LockFreeStack(const LockFreeStack&) = delete;
    LockFreeStack& operator=(const LockFreeStack&) = delete;

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
            return nullptr; // No hazard-pointer slots available (see README's known limitation).
        }

        Node* old_head = head.load();

        for (;;) {
            // Protect old_head, then re-verify head hasn't already moved
            // on before trusting that protection.
            Node* protected_candidate = old_head;
            hp->pointer.store(old_head);

            old_head = head.load();
            if (old_head != protected_candidate) {
                // head changed between our load and our hazard-pointer
                // store: restart the protect-then-verify sequence with
                // the fresh value rather than proceeding with a pointer
                // that might already be unprotected (see README bug #3).
                continue;
            }

            if (!old_head) {
                hp->pointer.store(nullptr);
                return nullptr;
            }

            if (head.compare_exchange_weak(old_head, old_head->next.load())) {
                break;
            }
        }

        hp->pointer.store(nullptr);

        std::unique_ptr<T> result;
        if (old_head->data.load()) {
            result = std::make_unique<T>(*old_head->data.load());
        }

        // Retire rather than delete immediately: another thread's hazard
        // pointer may still be protecting old_head.
        retire(old_head);

        // Periodically try to reclaim nodes that are no longer hazardous.
        if (std::hash<std::thread::id>{}(std::this_thread::get_id()) % 10 == 0) {
            delete_nodes_no_hazards();
        }

        return result;
    }

    bool empty() const { return head.load() == nullptr; }
};

template <typename T>
std::array<typename LockFreeStack<T>::HazardPointer, LockFreeStack<T>::MAX_THREADS>
    LockFreeStack<T>::hazard_pointers = {};

template <typename T>
std::atomic<typename LockFreeStack<T>::Node*> LockFreeStack<T>::to_be_deleted{nullptr};

int main() {
    LockFreeStack<int> stack;

    constexpr int kProducers = 4;
    constexpr int kItemsPerProducer = 5000;
    constexpr int kConsumers = 4;
    constexpr int kExpected = kProducers * kItemsPerProducer;

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&stack, p] {
            for (int i = 0; i < kItemsPerProducer; ++i) {
                stack.push(p * kItemsPerProducer + i);
            }
        });
    }
    for (auto& t : producers) t.join();

    std::atomic<int> popped_count{0};
    std::vector<std::thread> consumers;
    for (int c = 0; c < kConsumers; ++c) {
        consumers.emplace_back([&stack, &popped_count] {
            while (!stack.empty()) {
                if (auto value = stack.pop()) {
                    (void)value;
                    popped_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& t : consumers) t.join();

    std::cout << "Pushed " << kExpected << " items across " << kProducers << " producers.\n";
    std::cout << "Popped " << popped_count.load() << " items across " << kConsumers << " consumers.\n";
    std::cout << "Stack empty at end: " << std::boolalpha << stack.empty() << "\n";

    bool ok = popped_count.load() == kExpected && stack.empty();
    std::cout << (ok ? "PASS" : "FAIL") << "\n";

    return ok ? 0 : 1;
}
