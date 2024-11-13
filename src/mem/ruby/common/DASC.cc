#include "mem/ruby/common/DASC.hh"

namespace gem5
{

namespace ruby
{

void
DASC::addSharers(int nodeAct, int distance,
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
DASC::getSetSharersDasc()
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

void
DASC::print(std::ostream& out) const
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
