#include "mem/ruby/common/LPCBV.hh"

namespace gem5
{

namespace ruby
{

void
LPCBV::addToCBV(NodeID newSharer) {
  int group = newSharer / NODESPERGROUP;
  int i = 0;
  while ((group >= BITSPERPOINTER) && (i < MAXPOINTERS)) {
    i++;
    group -= BITSPERPOINTER;
  }
  assert(group < BITSPERPOINTER);
  assert(i < MAXPOINTERS);
  pointers[i].add(group);
}


void
LPCBV::print(std::ostream& out) const
{
    out << "[LPCBV (" << sharers.getSize() << ") ";

    for (int i = 0; i < sharers.getSize(); i++) {
        out << (bool) sharers.isElement(i) << " ";
    }
    out << "]";
}

}
}
