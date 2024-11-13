#ifndef __MEM_RUBY_COMMON_LPCBV_HH__
#define __MEM_RUBY_COMMON_LPCBV_HH__

#include "mem/ruby/common/NetDest.hh"
#include "mem/ruby/common/SharersImprecise.hh"

namespace gem5
{

namespace ruby
{

#define BITSPERPOINTER 8
#define NODESPERGROUP (NUMNODES / (BITSPERPOINTER*MAXPOINTERS))


class LPCBV : public AbstractSharersImprecise
{
  public:
    LPCBV()
    {
      sharers.setSize(NUMNODES);
      pointers.resize(MAXPOINTERS);
      for (int i = 0; i < MAXPOINTERS; i++) {
        pointers[i].setSize(BITSPERPOINTER);
      }
      type = Representation_Pointers;
    }
    ~LPCBV() {};
    void
    add(MachineID newSharer)
    {
      NetDest old = getSharers();
      if (type == Representation_Pointers) {
        sharers.add(newSharer.num);
        if (sharers.count() > MAXPOINTERS) {
          type = Representation_CoarseBitVector;
          for (int i = 0; i < sharers.getSize(); i++) {
            if (sharers.elementAt(i)) {
              addToCBV(i);
            }
          }
          sharers.clear();
        }
      }
      else addToCBV(newSharer.num);
      assert(getSharers().isElement(newSharer));
      assert(getSharers().isSuperset(old));
    }

    void
    remove(MachineID oldSharer)
    {
      if (type == Representation_Pointers) {
        sharers.remove(oldSharer.num);
      }
    }

    void
    clear()
    {
      sharers.clear();
      for (int i = 0; i < MAXPOINTERS; i++) {
        pointers[i].clear();
      }
      type = Representation_Pointers;
    }

    bool
    isBroadcast()
    {
      if (type == Representation_Pointers) {
        return 0;
      }
      for (int i = 0; i < MAXPOINTERS; i++) {
        if (!pointers[i].isBroadcast()) {
          return 0;
        }
      }
      return 1;
    }

    NetDest
    getSharers()
    {
      NetDest netSharers;
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
        netSharers.setNetDest(MACHINETYPE, sh);
      }
      else {
        netSharers.setNetDest(MACHINETYPE, sharers);
      }
      return netSharers;
    }
    void print(std::ostream& out) const;
  private:
    Set sharers;
    TypeRepresentation type;
    std::vector<Set> pointers;
    void addToCBV(NodeID newSharer);
};

inline std::ostream&
operator<<(std::ostream& out, const LPCBV& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

} // namespace ruby
} // namespace gem5


#endif // __MEM_RUBY_COMMON_LPCBV_HH__
