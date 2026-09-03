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

template <typename T> class LockFreeStack {
private:
  struct Node {
    std::atomic<T *> data_;
    std::atomic<Node *> next_;

    Node() : data_(nullptr), next_(nullptr) {}
  };

  std::atomic<Node *> head_;

  // Hazard pointers: protect against the ABA / use-after-free problem
  // that comes from freeing a node while another thread might still be
  // about to dereference it.
  static constexpr int kMaxThreads = 100;
  struct HazardPointer {
    std::atomic<std::thread::id> id_;
    std::atomic<Node *> pointer_;
  };

  static std::array<HazardPointer, kMaxThreads> g_hazard_pointers;
  static std::atomic<Node *> g_to_be_deleted;

  static HazardPointer *GetHazardPointerForCurrentThread() {
    thread_local static HazardPointer *hp = nullptr;

    if (!hp) {
      auto thread_id = std::this_thread::get_id();
      for (auto &hazard : g_hazard_pointers) {
        std::thread::id default_id;
        if (hazard.id_.compare_exchange_strong(default_id, thread_id)) {
          hp = &hazard;
          break;
        }
      }
    }
    return hp;
  }

  static bool IsPointerHazardous(Node *ptr) {
    for (const auto &hazard : g_hazard_pointers) {
      if (hazard.pointer_.load() == ptr) {
        return true;
      }
    }
    return false;
  }

  // Pushes `node` onto the to_be_deleted retire list.
  static void Retire(Node *node) {
    Node *old_retired_head = g_to_be_deleted.load();
    do {
      node->next_.store(old_retired_head);
    } while (!g_to_be_deleted.compare_exchange_weak(old_retired_head, node));
  }

  static void DeleteNodesNoHazards() {
    Node *current = g_to_be_deleted.exchange(nullptr);

    while (current) {
      Node *next = current->next_.load();

      if (!IsPointerHazardous(current)) {
        delete current->data_.load();
        delete current;
      } else {
        // Still protected by someone's hazard pointer: put it
        // back on the retire list for a later pass.
        Retire(current);
      }

      current = next;
    }
  }

public:
  LockFreeStack() : head_(nullptr) {}

  ~LockFreeStack() {
    while (Node *old_head = head_.load()) {
      head_.store(old_head->next_.load());
      delete old_head->data_.load();
      delete old_head;
    }

    DeleteNodesNoHazards();
  }

  LockFreeStack(const LockFreeStack &) = delete;
  LockFreeStack &operator=(const LockFreeStack &) = delete;

  void Push(T item) {
    Node *new_node = new Node;
    T *data = new T(std::move(item));
    new_node->data_.store(data);

    Node *current_head = head_.load();
    do {
      new_node->next_.store(current_head);
    } while (!head_.compare_exchange_weak(current_head, new_node));
  }

  std::unique_ptr<T> Pop() {
    HazardPointer *hp = GetHazardPointerForCurrentThread();
    if (!hp) {
      return nullptr; // No hazard-pointer slots available (see README's known
                      // limitation).
    }

    Node *old_head = head_.load();

    for (;;) {
      // Protect old_head, then re-verify head hasn't already moved
      // on before trusting that protection.
      Node *protected_candidate = old_head;
      hp->pointer_.store(old_head);

      old_head = head_.load();
      if (old_head != protected_candidate) {
        // head changed between our load and our hazard-pointer
        // store: restart the protect-then-verify sequence with
        // the fresh value rather than proceeding with a pointer
        // that might already be unprotected (see README bug #3).
        continue;
      }

      if (!old_head) {
        hp->pointer_.store(nullptr);
        return nullptr;
      }

      if (head_.compare_exchange_weak(old_head, old_head->next_.load())) {
        break;
      }
    }

    hp->pointer_.store(nullptr);

    std::unique_ptr<T> result;
    if (old_head->data_.load()) {
      result = std::make_unique<T>(*old_head->data_.load());
    }

    // Retire rather than delete immediately: another thread's hazard
    // pointer may still be protecting old_head.
    Retire(old_head);

    // Periodically try to reclaim nodes that are no longer hazardous.
    if (std::hash<std::thread::id>{}(std::this_thread::get_id()) % 10 == 0) {
      DeleteNodesNoHazards();
    }

    return result;
  }

  bool Empty() const { return head_.load() == nullptr; }
};

template <typename T>
std::array<typename LockFreeStack<T>::HazardPointer,
           LockFreeStack<T>::kMaxThreads>
    LockFreeStack<T>::g_hazard_pointers = {};

template <typename T>
std::atomic<typename LockFreeStack<T>::Node *>
    LockFreeStack<T>::g_to_be_deleted{nullptr};

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
        stack.Push(p * kItemsPerProducer + i);
      }
    });
  }
  for (auto &t : producers)
    t.join();

  std::atomic<int> popped_count{0};
  std::vector<std::thread> consumers;
  for (int c = 0; c < kConsumers; ++c) {
    consumers.emplace_back([&stack, &popped_count] {
      while (!stack.Empty()) {
        if (auto value = stack.Pop()) {
          (void)value;
          popped_count.fetch_add(1, std::memory_order_relaxed);
        }
      }
    });
  }
  for (auto &t : consumers)
    t.join();

  std::cout << "Pushed " << kExpected << " items across " << kProducers
            << " producers.\n";
  std::cout << "Popped " << popped_count.load() << " items across "
            << kConsumers << " consumers.\n";
  std::cout << "Stack empty at end: " << std::boolalpha << stack.Empty()
            << "\n";

  bool ok = popped_count.load() == kExpected && stack.Empty();
  std::cout << (ok ? "PASS" : "FAIL") << "\n";

  return ok ? 0 : 1;
}
