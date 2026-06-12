#pragma once

#include <array>
#include <cassert>
#include <cstddef>

template <typename T, std::size_t Size>
class RingBuffer 
{
    public:
    RingBuffer() : m_count(0) {}

    void push_back(const T& value) {
        if (m_count < Size) 
        {
            m_data[m_count] = value;
            m_count++;
        } 
        else 
        {
            std::copy(m_data.begin() + 1, m_data.end(), m_data.begin());
            m_data[Size - 1] = value;
        }
    }

    const T& operator[](std::size_t index) const 
    {
        assert(index < Size);
        return m_data[index];
    }

    T& operator[](std::size_t index) {
        assert(index < Size);
        return m_data[index];
    }

    inline const T& first() const { assert(m_count > 0); return m_data[0]; }
    inline T& first() { assert(m_count > 0); return m_data[0]; }
    inline const T& last() const { assert(m_count > 0); return m_data[m_count - 1]; }
    inline T& last() { assert(m_count > 0); return m_data[m_count - 1]; }

    inline std::size_t size() const { return m_count; }
    inline std::size_t capacity() const { return Size; }
    inline bool empty() const { return m_count == 0; }
    inline bool full() const { return m_count == Size; }
    inline void clear() { m_count = 0; }

    typename std::array<T, Size>::const_iterator begin() const { return m_data.begin(); }
    typename std::array<T, Size>::const_iterator end() const { return m_data.begin() + m_count; }
    
    private:
    std::array<T, Size> m_data;
    std::size_t m_count;
};