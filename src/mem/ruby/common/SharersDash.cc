#include "mem/ruby/common/SharersDash.hh"

#include "mem/ruby/system/RubySystem.hh"

namespace gem5
{

namespace ruby
{

#define MACHINETYPE MachineType_L1Cache
#define NUMNODES MachineType_base_count(MACHINETYPE)
#define NUMROWS 4 // RubySystem::getNetworkRows()
#define NUMCOLUMNS NUMNODES/NUMROWS

SharersDash::SharersDash()
{
  maxDistance = -1;
}

int
SharersDash::getDistanceToHome(MachineID newSharer)
{
  int distance = abs((home.num / NUMCOLUMNS) - (newSharer.num / NUMCOLUMNS));
  distance += abs((home.num % NUMCOLUMNS) - (newSharer.num % NUMCOLUMNS));
  return distance;
}

void
SharersDash::add(MachineID newSharer)
{
  if (maxDistance == -1) {
    home = newSharer;
    maxDistance = 0;
  }
  else {
    int distance = getDistanceToHome(newSharer);
    if (distance > maxDistance) {
      maxDistance = distance;
    }
  }
}

void
SharersDash::remove(MachineID oldSharer)
{
  if (maxDistance == 0) {
    assert(home == oldSharer);
    maxDistance = -1;
  }
}

void
SharersDash::clear()
{
  maxDistance = -1;
}

void
SharersDash::addSharers(int nodeAct, int distance,
                        Direction direction, Set* sharers)
{
  if (distance > 0) {
    if (((nodeAct / NUMCOLUMNS) > 0) && (direction != DOWN)) {
      int node = nodeAct - NUMCOLUMNS;
      sharers->add(node);
      addSharers(node, distance - 1, UP, sharers);
    }

    if (((nodeAct / NUMCOLUMNS) < (NUMROWS - 1)) && (direction != UP)) {
      int node = nodeAct + NUMCOLUMNS;
      sharers->add(node);
      addSharers(node, distance - 1, DOWN, sharers);
    }

    if (((nodeAct % NUMCOLUMNS) > 0) && (direction != RIGHT)) {
      int node = nodeAct - 1;
      sharers->add(node);
      addSharers(node, distance - 1, LEFT, sharers);
    }

    if (((nodeAct % NUMCOLUMNS) < (NUMCOLUMNS - 1)) && (direction != LEFT)) {
      int node = nodeAct + 1;
      sharers->add(node);
      addSharers(node, distance - 1, RIGHT, sharers);
    }
  }
}

Set
SharersDash::getSetSharers()
{
  Set sharers;
  sharers.setSize(NUMNODES);
  if (maxDistance > -1) {
    sharers.add(home.num);
  }
  if (maxDistance > 0) {
    if ((home.num / NUMCOLUMNS) > 0) {
      int node = home.num - NUMCOLUMNS;
      sharers.add(node);
      addSharers(node, maxDistance - 1, UP, &sharers);
    }

    if ((home.num / NUMCOLUMNS) < (NUMROWS - 1)) {
      int node = home.num + NUMCOLUMNS;
      sharers.add(node);
      addSharers(node, maxDistance - 1, DOWN, &sharers);
    }

    if ((home.num % NUMCOLUMNS) > 0) {
      int node = home.num - 1;
      sharers.add(node);
      addSharers(node, maxDistance - 1, LEFT, &sharers);
    }

    if ((home.num % NUMCOLUMNS) < (NUMCOLUMNS - 1)) {
      int node = home.num + 1;
      sharers.add(node);
      addSharers(node, maxDistance - 1, RIGHT, &sharers);
    }
  }
  return sharers;
}

bool
SharersDash::isBroadcast()
{
  return getSetSharers().isBroadcast();
}

NetDest
SharersDash::getSharers()
{
  NetDest netSharers;
  netSharers.setNetDest(MACHINETYPE, getSetSharers());
  return netSharers;
}

void
SharersDash::print(std::ostream& out) const
{
    out << "[SharersDash";

    /*for (int i = 0; i < NUMNODES; i++) {
      if (getDistanceToHome(i) <= maxDistance) {
        out << (bool) i << " ";
      }
    }*/
    out << "]";
}

}
}
