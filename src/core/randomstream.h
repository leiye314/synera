#pragma once
#include <QString>
#include <random>

// Per-game stream: never shares mutable randomness with another game or UI code.
class RandomStream
{
public:
    explicit RandomStream(quint32 seed = 0) { reset(seed); }
    void reset(quint32 seed);
    quint32 seed() const { return m_seed; }
    int bounded(int upperExclusive);
    QString encode() const;
    bool restore(quint32 seed, const QString& encoded);
private:
    quint32 m_seed = 0;
    std::mt19937 m_engine;
};
