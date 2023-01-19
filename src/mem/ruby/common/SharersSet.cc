#include "mem/ruby/common/SharersSet.hh"

namespace gem5
{

namespace ruby
{

#define MACHINETYPE MachineType_L1Cache

// #define MAXPOINTERS MachineType_base_count(MACHINETYPE)
#define MAXPOINTERS 1

SharersSet::SharersSet()
{
  resize();
  type = Representation_Pointers;
}

void
SharersSet::add(MachineID newSharer)
{
  if (type == Representation_Pointers) {
    sharers.add(newSharer.num);
    if (sharers.count() > MAXPOINTERS) {
      type = Representation_Broadcast;
      sharers.broadcast();
    }
  }
}

void
SharersSet::remove(MachineID oldSharer)
{
  if (type == Representation_Pointers) {
    sharers.remove(oldSharer.num);
  }
}

void
SharersSet::clear()
{
  sharers.clear();
  type = Representation_Pointers;
}

bool
SharersSet::isBroadcast()
{
  return type == Representation_Broadcast;
}

NetDest
SharersSet::getSharers()
{
  NetDest netSharers;
  netSharers.setNetDest(MACHINETYPE, sharers);
  return netSharers;
}

void
SharersSet::resize()
{
  sharers.setSize(MachineType_base_count(MACHINETYPE));
}


void
SharersSet::print(std::ostream& out) const
{
    out << "[SharersSet (" << sharers.getSize() << ") ";

    for (int i = 0; i < sharers.getSize(); i++) {
        out << (bool) sharers.isElement(i) << " ";
    }
    out << "]";
}

}
}
