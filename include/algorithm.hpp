#include <Arduino.h>

class MovingMean {
public:
    MovingMean(size_t size)
        : _size(size), _buffer(new float[size]()), _sum(0), _index(0), _count(0) {}

    ~MovingMean() {
        delete[] _buffer;
    }

    float update(float input) {
        // remove oldest value from sum
        _sum -= _buffer[_index];

        // store new value
        _buffer[_index] = input;
        _sum += input;

        // advance circular index
        _index = (_index + 1) % _size;

        // track fill level
        if (_count < _size) _count++;

        return get();
    }

    float get() const {
        return (_count == 0) ? 0.0f : (_sum / _count);
    }

private:
    size_t _size;
    float* _buffer;
    float _sum;
    size_t _index;
    size_t _count;
};