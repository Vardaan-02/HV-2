#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <type_traits>

namespace Engine::Utils {

// Fixed-capacity vector allocated purely on the stack (zero heap allocations)
template<typename T, std::size_t Capacity>
class StackVector {
public:
    using value_type      = T;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;
    using iterator        = T*;
    using const_iterator  = const T*;

    constexpr StackVector() noexcept = default;

    constexpr void push_back(const T& value) noexcept {
        assert(size_ < Capacity);
        data_[size_++] = value;
    }

    constexpr void push_back(T&& value) noexcept {
        assert(size_ < Capacity);
        data_[size_++] = std::move(value);
    }

    template<typename... Args>
    constexpr reference emplace_back(Args&&... args) noexcept {
        assert(size_ < Capacity);
        data_[size_] = T(std::forward<Args>(args)...);
        return data_[size_++];
    }

    constexpr void pop_back() noexcept {
        assert(size_ > 0);
        --size_;
    }

    constexpr void clear() noexcept { size_ = 0; }

    [[nodiscard]] constexpr pointer make_space(size_type count) noexcept {
        assert(size_ + count <= Capacity);
        pointer ptr = &data_[size_];
        size_ += count;
        return ptr;
    }

    [[nodiscard]] constexpr reference operator[](size_type index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    [[nodiscard]] constexpr const_reference operator[](size_type index) const noexcept {
        assert(index < size_);
        return data_[index];
    }

    [[nodiscard]] constexpr reference front() noexcept {
        assert(size_ > 0);
        return data_[0];
    }

    [[nodiscard]] constexpr const_reference front() const noexcept {
        assert(size_ > 0);
        return data_[0];
    }

    [[nodiscard]] constexpr reference back() noexcept {
        assert(size_ > 0);
        return data_[size_ - 1];
    }

    [[nodiscard]] constexpr const_reference back() const noexcept {
        assert(size_ > 0);
        return data_[size_ - 1];
    }

    [[nodiscard]] constexpr pointer data() noexcept { return data_; }
    [[nodiscard]] constexpr const_pointer data() const noexcept { return data_; }

    [[nodiscard]] constexpr iterator begin() noexcept { return data_; }
    [[nodiscard]] constexpr const_iterator begin() const noexcept { return data_; }
    [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return data_; }

    [[nodiscard]] constexpr iterator end() noexcept { return data_ + size_; }
    [[nodiscard]] constexpr const_iterator end() const noexcept { return data_ + size_; }
    [[nodiscard]] constexpr const_iterator cend() const noexcept { return data_ + size_; }

    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] constexpr size_type size() const noexcept { return size_; }
    [[nodiscard]] constexpr int ssize() const noexcept { return static_cast<int>(size_); }
    [[nodiscard]] static constexpr size_type capacity() noexcept { return Capacity; }

private:
    T data_[Capacity]{};
    size_type size_{0};
};

namespace Detail {

template<typename T, std::size_t Dim, std::size_t... Dims>
struct MultiArrayHelper {
    using ChildType = typename MultiArrayHelper<T, Dims...>::Type;
    using Type      = std::array<ChildType, Dim>;
};

template<typename T, std::size_t Dim>
struct MultiArrayHelper<T, Dim> {
    using Type = std::array<T, Dim>;
};

} // namespace Detail

// Modern N-dimensional contiguous array wrapper for history tables
template<typename T, std::size_t Dim, std::size_t... Dims>
class MultiArray {
public:
    using StorageType = typename Detail::MultiArrayHelper<T, Dim, Dims...>::Type;

    constexpr auto& operator[](std::size_t index) noexcept {
        assert(index < Dim);
        return data_[index];
    }

    constexpr const auto& operator[](std::size_t index) const noexcept {
        assert(index < Dim);
        return data_[index];
    }

    template<typename U>
    constexpr void fill(const U& val) noexcept {
        fill_recursive(data_, val);
    }

    [[nodiscard]] static constexpr std::size_t size() noexcept { return Dim; }

private:
    template<typename Arr, typename U>
    static constexpr void fill_recursive(Arr& arr, const U& val) noexcept {
        for (auto& item : arr) {
            if constexpr (requires { fill_recursive(item, val); }) {
                fill_recursive(item, val);
            } else {
                item = static_cast<T>(val);
            }
        }
    }

    StorageType data_{};
};

} // namespace Engine::Utils