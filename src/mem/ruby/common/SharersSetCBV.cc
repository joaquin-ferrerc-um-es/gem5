#include "mem/ruby/common/SharersSetCBV.hh"

#include "mem/ruby/system/RubySystem.hh"

namespace gem5
{

namespace ruby
{

#define MACHINETYPE MachineType_L1Cache
#define NUMNODES MachineType_base_count(MACHINETYPE)
# define MAXPOINTERS RubySystem::getLP()
# define BITSPERPOINTER 8
#define NODESPERGROUP (NUMNODES / (BITSPERPOINTER*MAXPOINTERS))

SharersSetCBV::SharersSetCBV()
{
  pointers = (Set*) malloc(sizeof(Set)*MAXPOINTERS);
  resize();
  type = Representation_PointersCBV;
}

void
SharersSetCBV::addToCBV(NodeID newSharer) {
  int group = newSharer / NODESPERGROUP;
  int i = 0;
  while ((group >= BITSPERPOINTER) && (i < MAXPOINTERS)) {
    i++;
    group -= BITSPERPOINTER;
  }
  assert(group < BITSPERPOINTER);
  pointers[i].add(group);
}

void
SharersSetCBV::representationToCBV() {
  type = Representation_CoarseBitVector;
  for (int i = 0; i < sharers.getSize(); i++) {
    if (sharers.elementAt(i)) {
      addToCBV(i);
    }
  }
  sharers.clear();
}

void
SharersSetCBV::add(MachineID newSharer)
{
  if (type == Representation_PointersCBV) {
    sharers.add(newSharer.num);
    if (sharers.count() > MAXPOINTERS) {
      representationToCBV();
    }
  }
  else addToCBV(newSharer.num);
}

void
SharersSetCBV::remove(MachineID oldSharer)
{
  if (type == Representation_PointersCBV) {
    sharers.remove(oldSharer.num);
  }
}

void
SharersSetCBV::clear()
{
  sharers.clear();
  for (int i = 0; i < MAXPOINTERS; i++) {
    pointers[i].clear();
  }
  type = Representation_PointersCBV;
}

bool
SharersSetCBV::isBroadcast()
{
  if (type == Representation_PointersCBV) {
    return 0;
  }
  for (int i = 0; i < MAXPOINTERS; i++) {
    if (!pointers[i].isBroadcast()) {
      return 0;
    }
  }
  return 1;
}

Set
SharersSetCBV::getSetSharers()
{
  if (type == Representation_CoarseBitVector) {
    Set sh;
    sh.setSize(NUMNODES);
    for (int i = 0; i < MAXPOINTERS; i++) {
      for (int j = 0; j < BITSPERPOINTER; j++) {
        if (pointers[i].isElement(j)) {
          for (int k = 0; k < NODESPERGROUP; k++) {
            sh.add(i*BITSPERPOINTER*NODESPERGROUP + j*NODESPERGROUP + k);
          }
        }
      }
    }
    return sh;
  }
  else {
    return sharers;
  }
}

NetDest
SharersSetCBV::getSharers()
{
  NetDest netSharers;
  netSharers.setNetDest(MACHINETYPE, getSetSharers());
  return netSharers;
}

void
SharersSetCBV::resize()
{
  sharers.setSize(NUMNODES);
  for (int i = 0; i < MAXPOINTERS; i++) {
    pointers[i].setSize(BITSPERPOINTER);
  }
}


void
SharersSetCBV::print(std::ostream& out) const
{
    out << "[SharersSetCBV (" << sharers.getSize() << ") ";

    for (int i = 0; i < sharers.getSize(); i++) {
        out << (bool) sharers.isElement(i) << " ";
    }
    out << "]";
}

}
}
