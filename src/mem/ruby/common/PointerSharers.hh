#ifndef __MEM_RUBY_COMMON_POINTERSHARERS_HH__
#define __MEM_RUBY_COMMON_POINTERSHARERS_HH__

#include "mem/ruby/common/NetDest.hh"

#define MAX_POINTERS 16

namespace gem5
{

namespace ruby
{

enum TypeRepresentation
{
    Representation_Pointers,
    Representation_Broadcast
};

// Pointer specifies a L1 cache
class PointerSharers
{
  public:
    PointerSharers();
    ~PointerSharers() {};
    void add(MachineID newSharer);
    void remove(MachineID oldSharer);
    void clear();
    bool isBroadcast();
    NetDest getSharers();
    void resize();
    void print(std::ostream& out) const;
  private:
    int maxPointers;
    Set sharers;

    NodeID bitIndex(NodeID index) const { return index; }
    TypeRepresentation type;
};

inline std::ostream&
operator<<(std::ostream& out, const PointerSharers& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

} // namespace ruby
} // namespace gem5


#endif // __MEM_RUBY_COMMON_POINTERSHARERS_HH__
