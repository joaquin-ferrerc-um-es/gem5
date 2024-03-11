#include "mem/ruby/common/SharersImprecise.hh"

#include "mem/ruby/system/RubySystem.hh"

namespace gem5
{

namespace ruby
{

SharersImprecise::SharersImprecise()
{
  if (IMPRECISEREPRESENTATION == "lp") {
    ImpreciseRep = LP;
  }
  else if (IMPRECISEREPRESENTATION == "coarse_bit_vector") {
    ImpreciseRep = CBV;
  }
  else if (IMPRECISEREPRESENTATION == "dasc") {
    ImpreciseRep = DASC;
  }
  else {
    assert(false);
  }

  switch (ImpreciseRep)
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
SharersImprecise::getDistanceToHome(MachineID newSharer)
{
  int distance = abs((home.num / NUMROWS) - (newSharer.num / NUMROWS));
  distance += abs((home.num % NUMROWS) - (newSharer.num % NUMROWS));
  assert(distance <= MAXDISTANCE);
  return distance;
}

void
SharersImprecise::addToCBV(NodeID newSharer) {
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
SharersImprecise::representationToCBV() {
  typeCBV = Representation_CoarseBitVector;
  for (int i = 0; i < sharers.getSize(); i++) {
    if (sharers.elementAt(i)) {
      addToCBV(i);
    }
  }
  sharers.clear();
}

void
SharersImprecise::add(MachineID newSharer)
{
  NetDest old = getSharers();
  switch (ImpreciseRep)
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
  assert(getSharers().isSuperset(old));
}

void
SharersImprecise::remove(MachineID oldSharer)
{
  switch (ImpreciseRep)
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
SharersImprecise::clear()
{
  switch (ImpreciseRep)
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
SharersImprecise::getSetSharersCBV()
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
SharersImprecise::addSharers(int nodeAct, int distance,
                        Direction direction, Set* sh)
{
  if (distance > 0) {
    if (((nodeAct / NUMROWS) > 0) && (direction != DOWN)) {
      int node = nodeAct - NUMROWS;
      assert(node < NUMNODES);
      sh->add(node);
      addSharers(node, distance - 1, UP, sh);
    }

    if (((nodeAct / NUMROWS) < (NUMCOLUMNS - 1)) && (direction != UP)) {
      int node = nodeAct + NUMROWS;
      assert(node < NUMNODES);
      sh->add(node);
      addSharers(node, distance - 1, DOWN, sh);
    }

    if (((nodeAct % NUMROWS) > 0) && (direction != RIGHT)) {
      int node = nodeAct - 1;
      assert(node < NUMNODES);
      sh->add(node);
      addSharers(node, distance - 1, LEFT, sh);
    }

    if (((nodeAct % NUMROWS) < (NUMROWS - 1)) && (direction != LEFT)) {
      int node = nodeAct + 1;
      assert(node < NUMNODES);
      sh->add(node);
      addSharers(node, distance - 1, RIGHT, sh);
    }
  }
}

Set
SharersImprecise::getSetSharersDasc()
{
  Set sh;
  sh.setSize(NUMNODES);
  if (maxDistance > -1) {
    sh.add(home.num);
  }
  if (maxDistance > 0) {
    if ((home.num / NUMROWS) > 0) {
      int node = home.num - NUMROWS;
      assert(node < NUMNODES);
      sh.add(node);
      addSharers(node, maxDistance - 1, UP, &sh);
    }

    if ((home.num / NUMROWS) < (NUMCOLUMNS - 1)) {
      int node = home.num + NUMROWS;
      assert(node < NUMNODES);
      sh.add(node);
      addSharers(node, maxDistance - 1, DOWN, &sh);
    }

    if ((home.num % NUMROWS) > 0) {
      int node = home.num - 1;
      assert(node < NUMNODES);
      sh.add(node);
      addSharers(node, maxDistance - 1, LEFT, &sh);
    }

    if ((home.num % NUMROWS) < (NUMROWS - 1)) {
      int node = home.num + 1;
      assert(node < NUMNODES);
      sh.add(node);
      addSharers(node, maxDistance - 1, RIGHT, &sh);
    }
  }
  return sh;
}

bool
SharersImprecise::isBroadcast()
{
  switch (ImpreciseRep)
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
SharersImprecise::getSharers()
{
  NetDest netSharers;
  switch (ImpreciseRep)
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
    assert(false);
    break;
  }
  return netSharers;
}

void
SharersImprecise::resize()
{
  switch (ImpreciseRep)
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
SharersImprecise::print(std::ostream& out) const
{
    switch (ImpreciseRep)
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
