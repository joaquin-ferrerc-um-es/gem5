#include "mem/ruby/common/SharersJFC.hh"

#include "mem/ruby/system/RubySystem.hh"

namespace gem5
{

namespace ruby
{

#define MACHINETYPE MachineType_L1Cache
#define NUMNODES MachineType_base_count(MACHINETYPE)
#define JFCREPRESENTATION RubySystem::getJFCRepresentation()

// Sharers Set
#define MAXPOINTERS RubySystem::getLP()

// Sharers CBV
#define BITSPERPOINTER 8
#define NODESPERGROUP (NUMNODES / (BITSPERPOINTER*MAXPOINTERS))

// Sharers Dash
#define NUMROWS RubySystem::getNetworkRows()
#define NUMCOLUMNS (NUMNODES/NUMROWS)


SharersJFC::SharersJFC()
{
  if (JFCREPRESENTATION == "lp") {
    JFCRep = LP;
  }
  else if (JFCREPRESENTATION == "coarse_bit_vector") {
    JFCRep = CBV;
  }
  else if (JFCREPRESENTATION == "dash") {
    JFCRep = DASH;
  }
  else {
    assert(1 == 0);
  }

  switch (JFCRep)
  {
  case LP:
  {
    resize();
    type = Representation_Pointers;
    break;
  }

  case CBV:
  {
    pointers = (Set*) malloc(sizeof(Set)*MAXPOINTERS);
    resize();
    typeCBV = Representation_PointersCBV;
    break;
  }

  case DASH:
  {
    maxDistance = -1;
    break;
  }

  default:
    assert(1 == 0);
    break;
  }
}


SharersJFC::~SharersJFC()
{
  switch (JFCRep)
  {
  case LP:
    break;

  case CBV:
    free(pointers);
    break;

  case DASH:
    break;

  default:
    assert(1 == 0);
    break;
  }
}

int
SharersJFC::getDistanceToHome(MachineID newSharer)
{
  int distance = abs((home.num / NUMCOLUMNS) - (newSharer.num / NUMCOLUMNS));
  distance += abs((home.num % NUMCOLUMNS) - (newSharer.num % NUMCOLUMNS));
  return distance;
}

void
SharersJFC::addToCBV(NodeID newSharer) {
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
SharersJFC::representationToCBV() {
  typeCBV = Representation_CoarseBitVector;
  for (int i = 0; i < sharers.getSize(); i++) {
    if (sharers.elementAt(i)) {
      addToCBV(i);
    }
  }
  sharers.clear();
}

void
SharersJFC::add(MachineID newSharer)
{
  switch (JFCRep)
  {
  case LP:
  {
    if (type == Representation_Pointers) {
      sharers.add(newSharer.num);
      if (sharers.count() > MAXPOINTERS) {
        type = Representation_Broadcast;
        sharers.broadcast();
      }
    }
    break;
  }

  case CBV:
  {
    if (typeCBV == Representation_PointersCBV) {
      sharers.add(newSharer.num);
      if (sharers.count() > MAXPOINTERS) {
        representationToCBV();
      }
    }
    else addToCBV(newSharer.num);
    break;
  }

  case DASH:
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
    break;
  }

  default:
    assert(1 == 0);
    break;
  }
}

void
SharersJFC::remove(MachineID oldSharer)
{
  switch (JFCRep)
  {
  case LP:
  {
    if (type == Representation_Pointers) {
      sharers.remove(oldSharer.num);
    }
    break;
  }

  case CBV:
  {
    if (typeCBV == Representation_PointersCBV) {
      sharers.remove(oldSharer.num);
    }
    break;
  }

  case DASH:
  {
    if (maxDistance == 0) {
      assert(home == oldSharer);
      maxDistance = -1;
    }
    break;
  }

  default:
    assert(1 == 0);
    break;
  }
}

void
SharersJFC::clear()
{
  switch (JFCRep)
  {
  case LP:
  {
    sharers.clear();
    type = Representation_Pointers;
    break;
  }

  case CBV:
  {
    sharers.clear();
    for (int i = 0; i < MAXPOINTERS; i++) {
      pointers[i].clear();
    }
    typeCBV = Representation_PointersCBV;
    break;
  }

  case DASH:
  {
    maxDistance = -1;
    break;
  }

  default:
    assert(1 == 0);
    break;
  }
}

Set
SharersJFC::getSetSharersCBV()
{
  if (typeCBV == Representation_CoarseBitVector) {
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

void
SharersJFC::addSharers(int nodeAct, int distance,
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
SharersJFC::getSetSharersDash()
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
SharersJFC::isBroadcast()
{
  switch (JFCRep)
  {
  case LP:
  {
    return type == Representation_Broadcast;
    break;
  }

  case CBV:
  {
    if (typeCBV == Representation_PointersCBV) {
      return 0;
    }
    for (int i = 0; i < MAXPOINTERS; i++) {
      if (!pointers[i].isBroadcast()) {
        return 0;
      }
    }
    return 1;
    break;
  }

  case DASH:
  {
    return getSetSharersDash().isBroadcast();
    break;
  }

  default:
    assert(1 == 0);
    break;
  }
}

NetDest
SharersJFC::getSharers()
{
  NetDest netSharers;
  switch (JFCRep)
  {
  case LP:
  {
    netSharers.setNetDest(MACHINETYPE, sharers);
    break;
  }

  case CBV:
  {
    netSharers.setNetDest(MACHINETYPE, getSetSharersCBV());
    break;
  }

  case DASH:
  {
    netSharers.setNetDest(MACHINETYPE, getSetSharersDash());
    break;
  }

  default:
    assert(1 == 0);
    break;
  }
  return netSharers;
}

void
SharersJFC::resize()
{
  switch (JFCRep)
  {
  case LP:
  {
    sharers.setSize(NUMNODES);
    break;
  }

  case CBV:
  {
    sharers.setSize(NUMNODES);
    for (int i = 0; i < MAXPOINTERS; i++) {
      pointers[i].setSize(BITSPERPOINTER);
    }
    break;
  }

  case DASH:
  {
  }
    break;

  default:
    assert(1 == 0);
    break;
  }
}

void
SharersJFC::print(std::ostream& out) const
{
    switch (JFCRep)
    {
    case LP:
    {
      out << "[SharersSet (" << sharers.getSize() << ") ";

      for (int i = 0; i < sharers.getSize(); i++) {
          out << (bool) sharers.isElement(i) << " ";
      }
      out << "]";
      break;
    }

    case CBV:
    {
      out << "[SharersSetCBV (" << sharers.getSize() << ") ";

      for (int i = 0; i < sharers.getSize(); i++) {
          out << (bool) sharers.isElement(i) << " ";
      }
      out << "]";
      break;
    }

    case DASH:
    {
      out << "[SharersDash";

      /*for (int i = 0; i < NUMNODES; i++) {
        if (getDistanceToHome(i) <= maxDistance) {
          out << (bool) i << " ";
        }
      }*/
      out << "]";
    }
      break;

    default:
      assert(1 == 0);
      break;
    }
}

}
}
