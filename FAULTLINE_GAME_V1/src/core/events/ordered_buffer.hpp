#pragma once
#include <utility>
#include <vector>

namespace faultline::core {
// Per-tick insertion order. Draining first isolates commands emitted by a consumer.
template<class T> class OrderedBuffer {
public:
    void push(T value) { values_.push_back(std::move(value)); }
    [[nodiscard]] const std::vector<T>& values() const { return values_; }
    [[nodiscard]] bool empty() const { return values_.empty(); }
    void clear() { values_.clear(); }
    [[nodiscard]] std::vector<T> drain() {
        std::vector<T> result; result.swap(values_); return result;
    }
    template<class Consumer> void commit(Consumer&& consume) {
        for (const auto& value : drain()) consume(value);
    }
private:
    std::vector<T> values_;
};
}
