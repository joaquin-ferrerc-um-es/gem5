#ifndef __MEM_RUBY_COMMON_SHARERSSETCBV_HH__
#define __MEM_RUBY_COMMON_SHARERSSETCBV_HH__

#include "mem/ruby/common/NetDest.hh"

namespace gem5
{

namespace ruby
{

enum TypeRepresentationCBV
{
    Representation_PointersCBV,
    Representation_CoarseBitVector
};


class SharersSetCBV
{
  public:
    SharersSetCBV();
    ~SharersSetCBV() { free(pointers); };
    void add(MachineID newSharer);
    void remove(MachineID oldSharer);
    void clear();
    bool isBroadcast();
    NetDest getSharers();
    void resize();
    void print(std::ostream& out) const;
  private:
    Set sharers;
    Set *pointers;
    TypeRepresentationCBV type;
    void addToCBV(NodeID newSharer);
    void representationToCBV();
    Set getSetSharers();
};

inline std::ostream&
operator<<(std::ostream& out, const SharersSetCBV& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

} // namespace ruby
} // namespace gem5


#endif // __MEM_RUBY_COMMON_SHARERSSETCBV_HH__
