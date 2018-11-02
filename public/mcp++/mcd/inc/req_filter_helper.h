#ifndef REQ_FILTER_HELPER_H
#define REQ_FILTER_HELPER_H

namespace mcppp {
namespace check_overload {

class InfoItem {
public:
    InfoItem() : m_count(0) {
        m_info = new std::string("");
    }
    InfoItem(uint32_t count, const std::string& str) : m_count(count) {
        m_info = new std::string(str);
    }
    InfoItem(const InfoItem& item) : m_count(item.m_count) {
        m_info = new std::string(*(item.m_info));
    }
    ~InfoItem() {
        delete m_info;
    }
    InfoItem& operator=(InfoItem& item) {
        m_count = item.m_count;
        m_info->swap(*(item.m_info));
        return *this;
    }
    bool operator<(const InfoItem& item) {
        return m_count < item.m_count;
    }
    bool operator>(const InfoItem& item) {
        return m_count > item.m_count;
    }

public:
    friend std::ostream& operator<<(std::ostream& os, const InfoItem& item);
    friend class ReqFilter;

private:
    uint32_t m_count;
    std::string *m_info;
};

std::ostream& operator<<(std::ostream& os, const InfoItem& item);

template<class T>
class HeapSort {
public:
    HeapSort(uint32_t item_num) : m_array_len(item_num) {
        m_array = new T[item_num];
    }
    ~HeapSort() { delete[] m_array; }
    ////////////////////////////////////////////////////////
    void HeapInit() {
        // memset(m_array, 0x0, sizeof(T) * m_array_len);
    }
    void Update(T& value) {
        if (value > m_array[0]) {
            m_array[0] = value;
            AdjustHeap(m_array, m_array_len, 0);
        }
    }
    void TopK() {
        AdjustSequence(m_array, m_array_len);
    }
    void PrintArray() {
        for (uint32_t i = 0; i < m_array_len; i++) {
            std::cout << m_array[i] << " ";
        }
        std::cout << "\n";
    }
    ////////////////////////////////////////////////////////
    void Sort(T *array, uint32_t len) {
        BuildHeap(array, len);
        AdjustSequence(array, len);
    }
    void BuildHeap(T *array, uint32_t len) {
        for (int32_t i = len / 2 - 1; i >= 0; i--) {
            AdjustHeap(array, len, i);
        }
    }
    void AdjustSequence(T *array, uint32_t len) {
        for (uint32_t i = len - 1; i >= 1; i--) {
            T temp = array[i];
            array[i] = array[0];
            array[0] = temp;
            AdjustHeap(array, i, 0);
        }
    }
    void AdjustHeap(T *array, uint32_t len, uint32_t index) {
        uint32_t left = 2 * index + 1;
        uint32_t right = 2 * index + 2;
        uint32_t least = index;

        if (left < len && array[least] > array[left]) {
            least = left;
        }
        if (right < len && array[least] > array[right]) {
            least = right;
        }
        if (least != index) {
            T temp = array[least];
            array[least] = array[index];
            array[index] = temp;

            AdjustHeap(array, len, least);
        }
    }

public:
    friend class ReqFilter;

private:
    uint32_t m_array_len;
    T *m_array;
};

class StrHash {
public:
    size_t operator ()(const std::string& str) const {
        return BKDRHash(str);
        // return APHash(str);
    }
    // BKDR Hash Function
    inline unsigned int BKDRHash(const std::string& str) const {
        unsigned int seed = 131; // 31 131 1313 13131 131313 etc..
        unsigned int hash = 0;
        for (uint32_t i = 0; i < str.size(); i++) {
            hash = hash * seed + str[i];
        }
        return (hash & 0x7FFFFFFF);
    }
    // AP Hash Function
    inline unsigned int APHash(const std::string& str) const {
        unsigned int hash = 0;
        for (uint32_t i = 0; i < str.size(); i++) {
            if ((i & 1) == 0) {
                hash ^= ((hash << 7) ^ str[i] ^ (hash >> 3));
            } else {
                hash ^= (~((hash << 11) ^ str[i] ^ (hash >> 5)));
            }
        }
        return (hash & 0x7FFFFFFF);
    }
};

class StrEqual {
public:
    bool operator ()(const std::string& str1, const std::string& str2) const {
        int ret = str1.compare(str2);
        return ret == 0 ? true : false;
    }
};

} // namespace check_overload
} // namespace mcppp

#endif  // REQ_FILTER_HELPER_H
