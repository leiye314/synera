#include "core/randomstream.h"
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

void RandomStream::reset(quint32 seed)
{
    m_seed = seed;
    m_engine.seed(seed);
}
int RandomStream::bounded(int upperExclusive)
{
    Q_ASSERT(upperExclusive > 0);
    if (upperExclusive <= 0) return 0;
    const auto bound = static_cast<quint32>(upperExclusive);
    // Rejection sampling defines the mapping independently of standard-library distributions.
    const quint32 threshold = (quint32(0) - bound) % bound;
    quint32 value;
    do { value = static_cast<quint32>(m_engine()); } while (value < threshold);
    return static_cast<int>(value % bound);
}
QString RandomStream::encode() const
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << m_engine;
    return QString::fromStdString(out.str());
}
bool RandomStream::restore(quint32 seed, const QString& encoded)
{
    if (encoded.isEmpty() || encoded.size() > 10000) return false;
    std::istringstream in(encoded.toStdString());
    in.imbue(std::locale::classic());
    std::mt19937 candidate;
    try {
        if (!(in >> candidate)) return false;
    } catch (const std::invalid_argument&) {
        // MSVC throws for malformed engine input instead of only setting failbit.
        return false;
    }
    in >> std::ws;
    if (!in.eof()) return false;
    std::ostringstream canonical;
    canonical.imbue(std::locale::classic());
    canonical << candidate;
    if (QString::fromStdString(canonical.str()) != encoded) return false;
    m_seed = seed;
    m_engine = candidate;
    return true;
}
