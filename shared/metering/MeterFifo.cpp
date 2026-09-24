#include "MeterFifo.h"

MeterFifo::MeterFifo(int capacity)
    : fifo(capacity), bufferX((size_t) capacity), bufferY((size_t) capacity)
{
}

void MeterFifo::push(float x, float y) noexcept
{
    int start1, size1, start2, size2;
    fifo.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 > 0) // 0 means the FIFO is full: drop this point
    {
        bufferX[(size_t) start1] = x;
        bufferY[(size_t) start1] = y;
    }
    fifo.finishedWrite(size1 + size2);
}

void MeterFifo::drainInto(std::vector<float>& outX, std::vector<float>& outY)
{
    outX.clear();
    outY.clear();

    const int numReady = fifo.getNumReady();
    if (numReady <= 0)
        return;

    int start1, size1, start2, size2;
    fifo.prepareToRead(numReady, start1, size1, start2, size2);

    outX.reserve((size_t) numReady);
    outY.reserve((size_t) numReady);
    for (int i = 0; i < size1; ++i)
    {
        outX.push_back(bufferX[(size_t) (start1 + i)]);
        outY.push_back(bufferY[(size_t) (start1 + i)]);
    }
    for (int i = 0; i < size2; ++i)
    {
        outX.push_back(bufferX[(size_t) (start2 + i)]);
        outY.push_back(bufferY[(size_t) (start2 + i)]);
    }

    fifo.finishedRead(size1 + size2);
}
