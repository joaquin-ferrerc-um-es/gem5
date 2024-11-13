#ifndef __MEM_RUBY_COMMON_LPBROADCAST_HH__
#define __MEM_RUBY_COMMON_LPBROADCAST_HH__

#include "mem/ruby/common/NetDest.hh"
#include "mem/ruby/common/SharersImprecise.hh"

namespace gem5
{

namespace ruby
{

class LPBroadcast : public AbstractSharersImprecise
{
  public:
    LPBroadcast()
    {
      sharers.setSize(NUMNODES);
      type = Representation_Pointers;
    }
    ~LPBroadcast() {};
    void
    add(MachineID newSharer)
    {
      NetDest old = getSharers();
      if (type == Representation_Pointers) {
        sharers.add(newSharer.num);
        if (sharers.count() > MAXPOINTERS) {
          type = Representation_Broadcast;
          sharers.broadcast();
        }
      }
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
      type = Representation_Pointers;
    }

    bool
    isBroadcast()
    {
      return type == Representation_Broadcast;
    }

    NetDest
    getSharers()
    {
      NetDest netSharers;
      netSharers.setNetDest(MACHINETYPE, sharers);
      return netSharers;
    }
    void print(std::ostream& out) const;
  private:
    Set sharers;
    TypeRepresentation type;
};

inline std::ostream&
operator<<(std::ostream& out, const LPBroadcast& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

} // namespace ruby
} // namespace gem5


#endif // __MEM_RUBY_COMMON_LPBROADCAST_HH__
