#include "mem/ruby/common/LPBroadcast.hh"

namespace gem5
{

namespace ruby
{

void
LPBroadcast::print(std::ostream& out) const
{
    out << "[LPBroadcast (" << sharers.getSize() << ") ";

    for (int i = 0; i < sharers.getSize(); i++) {
        out << (bool) sharers.isElement(i) << " ";
    }
    out << "]";
}

}
}
