#include "mem/ruby/common/SharersJFC.hh"

#include "mem/ruby/system/RubySystem.hh"

namespace gem5
{

namespace ruby
{

SharersJFC::SharersJFC()
{
  if (JFCREPRESENTATION == "lp") {
    JFCRep = LP;
  }
  else if (JFCREPRESENTATION == "coarse_bit_vector") {
    JFCRep = CBV;
  }
  else if (JFCREPRESENTATION == "dasc") {
    JFCRep = DASC;
  }
  else {
    assert(false);
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
    resize();
    typeCBV = Representation_PointersCBV;
    break;
  }

  case DASC:
  {
    maxDistance = -1;
    break;
  }

  default:
    assert(false);
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
  assert(i < MAXPOINTERS);
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

  case DASC:
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
    assert(false);
    break;
  }
  assert(getSharers().isElement(newSharer));
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

  case DASC:
  {
    if (maxDistance == 0) {
      assert(home == oldSharer);
      maxDistance = -1;
    }
    break;
  }

  default:
    assert(false);
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

  case DASC:
  {
    maxDistance = -1;
    break;
  }

  default:
    assert(false);
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
                        Direction direction, Set* sh)
{
  if (distance > 0) {
    if (((nodeAct / NUMCOLUMNS) > 0) && (direction != DOWN)) {
      int node = nodeAct - NUMCOLUMNS;
      sh->add(node);
      addSharers(node, distance - 1, UP, sh);
    }

    if (((nodeAct / NUMCOLUMNS) < (NUMROWS - 1)) && (direction != UP)) {
      int node = nodeAct + NUMCOLUMNS;
      sh->add(node);
      addSharers(node, distance - 1, DOWN, sh);
    }

    if (((nodeAct % NUMCOLUMNS) > 0) && (direction != RIGHT)) {
      int node = nodeAct - 1;
      sh->add(node);
      addSharers(node, distance - 1, LEFT, sh);
    }

    if (((nodeAct % NUMCOLUMNS) < (NUMCOLUMNS - 1)) && (direction != LEFT)) {
      int node = nodeAct + 1;
      sh->add(node);
      addSharers(node, distance - 1, RIGHT, sh);
    }
  }
}

Set
SharersJFC::getSetSharersDasc()
{
  Set sh;
  sh.setSize(NUMNODES);
  if (maxDistance > -1) {
    sh.add(home.num);
  }
  if (maxDistance > 0) {
    if ((home.num / NUMCOLUMNS) > 0) {
      int node = home.num - NUMCOLUMNS;
      sh.add(node);
      addSharers(node, maxDistance - 1, UP, &sh);
    }

    if ((home.num / NUMCOLUMNS) < (NUMROWS - 1)) {
      int node = home.num + NUMCOLUMNS;
      sh.add(node);
      addSharers(node, maxDistance - 1, DOWN, &sh);
    }

    if ((home.num % NUMCOLUMNS) > 0) {
      int node = home.num - 1;
      sh.add(node);
      addSharers(node, maxDistance - 1, LEFT, &sh);
    }

    if ((home.num % NUMCOLUMNS) < (NUMCOLUMNS - 1)) {
      int node = home.num + 1;
      sh.add(node);
      addSharers(node, maxDistance - 1, RIGHT, &sh);
    }
  }
  return sh;
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

  case DASC:
  {
    return getSetSharersDasc().isBroadcast();
    break;
  }

  default:
    assert(false);
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

  case DASC:
  {
    netSharers.setNetDest(MACHINETYPE, getSetSharersDasc());
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
    pointers.resize(MAXPOINTERS);
    for (int i = 0; i < MAXPOINTERS; i++) {
      pointers[i].setSize(BITSPERPOINTER);
    }
    break;
  }

  case DASC:
  {
    break;
  }

  default:
    assert(false);
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

    case DASC:
    {
      out << "[SharersDash";

      /*for (int i = 0; i < NUMNODES; i++) {
        if (getDistanceToHome(i) <= maxDistance) {
          out << (bool) i << " ";
        }
      }*/
      out << "]";
      break;
    }

    default:
      assert(false);
      break;
    }
}

}
}
