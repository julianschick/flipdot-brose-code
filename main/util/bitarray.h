#ifndef BITARRAY_H
#define BITARRAY_H

#include "../libs/globals.h"

class BitArray {

public:
    BitArray(int size_);

    BitArray(const BitArray &other);

    ~BitArray();

    bool get(int index) const;
    void set(int index, bool value);
    void setAll(bool value);

    void set8(int index, uint8_t value);

    void copy_from(const BitArray& other);
    void copy_from(uint8_t* buffer, size_t len);

    bool transition_vector_to(const class BitArray & other, int8_t* transition_vector);

private:
    uint32_t masks[32];

    int size;
    int data_size;
    uint32_t* data;

    inline bool range_check(int index) const;

};


#endif //BITARRAY_H
