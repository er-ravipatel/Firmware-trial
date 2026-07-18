// Fixed-capacity ring buffer — statically sized, no heap.
// Single-consumer/single-producer under Circle's cooperative scheduler (no preemption),
// so no locking is required as long as producers and the consumer only yield at task
// boundaries. See IMPLEMENTATION §3.3.
#pragma once
#include <stddef.h>

namespace lf {

template <class T, size_t Capacity>
class RingBuffer {
    static_assert(Capacity > 0, "RingBuffer capacity must be > 0");

public:
    bool empty() const { return size_ == 0; }
    bool full() const { return size_ == Capacity; }
    size_t size() const { return size_; }
    static constexpr size_t capacity() { return Capacity; }

    // Returns false if full (does not overwrite).
    bool push(const T& v) {
        if (full()) return false;
        buf_[tail_] = v;
        tail_ = (tail_ + 1) % Capacity;
        ++size_;
        return true;
    }

    // Drop the oldest element if full, then push. For non-critical events that must
    // never block a producer (IMPLEMENTATION §2.2).
    void push_overwrite(const T& v) {
        if (full()) {
            T discard;
            pop(discard);
        }
        push(v);
    }

    // Returns false if empty.
    bool pop(T& out) {
        if (empty()) return false;
        out = buf_[head_];
        head_ = (head_ + 1) % Capacity;
        --size_;
        return true;
    }

    void clear() { head_ = tail_ = size_ = 0; }

private:
    T buf_[Capacity];
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t size_ = 0;
};

}  // namespace lf
