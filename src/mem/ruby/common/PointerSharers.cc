#include "mem/ruby/common/PointerSharers.hh"

namespace gem5
{

namespace ruby
{
PointerSharers::PointerSharers()
{
  resize();
  maxPointers = MachineType_base_count(MachineType_L1Cache);
  type = Representation_Pointers;
}

void
PointerSharers::add(MachineID newSharer)
{
  if (type == Representation_Pointers) {
    sharers.add(bitIndex(newSharer.num));
    if (sharers.count() >= maxPointers) {
      type = Representation_Broadcast;
      sharers.broadcast();
    }
  }
}

void
PointerSharers::remove(MachineID oldSharer)
{
  if (type == Representation_Pointers) {
    sharers.remove(bitIndex(oldSharer.num));
  }
}

void
PointerSharers::clear()
{
  sharers.clear();
  type = Representation_Pointers;
}

bool
PointerSharers::isBroadcast()
{
  return type == Representation_Broadcast;
}

NetDest
PointerSharers::getSharers()
{
  NetDest netSharers;
  netSharers.setNetDest(MachineType_L1Cache, sharers);
  return netSharers;
}

void
PointerSharers::resize()
{
  sharers.setSize(MachineType_base_count(MachineType_L1Cache));
}


void
PointerSharers::print(std::ostream& out) const
{
    out << "[PointerSharers (" << sharers.getSize() << ") ";

    for (int i = 0; i < sharers.getSize(); i++) {
        out << (bool) sharers.isElement(i) << " ";
    }
    out << "]";
}

}
}
