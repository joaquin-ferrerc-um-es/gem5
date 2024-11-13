#ifndef __MEM_RUBY_COMMON_DASC_HH__
#define __MEM_RUBY_COMMON_DASC_HH__

#include "mem/ruby/common/NetDest.hh"
#include "mem/ruby/common/SharersImprecise.hh"

namespace gem5
{

namespace ruby
{

#define NUMROWS RubySystem::getNetworkRows()
#define NUMCOLUMNS (NUMNODES/NUMROWS)
#define MAXDISTANCE ((NUMROWS - 1) + (NUMCOLUMNS - 1))

enum Direction
{
  UP, DOWN, LEFT, RIGHT
};

class DASC : public AbstractSharersImprecise
{
  public:
    DASC()
    {
      maxDistance = -1;
    }
    ~DASC() {};
    void
    add(MachineID newSharer)
    {
      NetDest old = getSharers();
      if (maxDistance == -1) {
        home = newSharer;
        maxDistance = 0;
      }
      else {
        int distance = abs((int)(home.num / NUMROWS) - (int)(newSharer.num / NUMROWS));
        distance += abs((int)(home.num % NUMROWS) - (int)(newSharer.num % NUMROWS));
        assert(distance <= MAXDISTANCE);
        if (distance > maxDistance) {
          maxDistance = distance;
        }
      }
      assert(getSharers().isElement(newSharer));
      assert(getSharers().isSuperset(old));
    }

    void
    remove(MachineID oldSharer)
    {
      if (maxDistance == 0) {
        assert(home == oldSharer);
        maxDistance = -1;
      }
    }

    void
    clear()
    {
      maxDistance = -1;
    }
    bool
    isBroadcast()
    {
      return getSetSharersDasc().isBroadcast();
    }

    NetDest
    getSharers()
    {
      NetDest netSharers;
      netSharers.setNetDest(MACHINETYPE, getSetSharersDasc());
      return netSharers;
    }
    void print(std::ostream& out) const;
  private:
    void addSharers(int node, int distance, Direction direction, Set *sh);
    Set getSetSharersDasc();
    MachineID home;
    int maxDistance;
};

inline std::ostream&
operator<<(std::ostream& out, const DASC& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

} // namespace ruby
} // namespace gem5


#endif // __MEM_RUBY_COMMON_DASC_HH__
