#ifndef __MEM_RUBY_COMMON_SHARERSIMPRECISE_HH__
#define __MEM_RUBY_COMMON_SHARERSIMPRECISE_HH__

#include "mem/ruby/common/NetDest.hh"
#include "mem/ruby/system/RubySystem.hh"

namespace gem5
{

namespace ruby
{

#define MACHINETYPE MachineType_L1Cache
#define NUMNODES MachineType_base_count(MACHINETYPE)

// Sharers Set
#define MAXPOINTERS RubySystem::getLP()

enum TypeRepresentation
{
    Representation_Pointers,
    Representation_Broadcast,
    Representation_CoarseBitVector
};

class AbstractSharersImprecise
{
  public:
    AbstractSharersImprecise() {};
    ~AbstractSharersImprecise() {};
    virtual void add(MachineID newSharer) {};
    virtual void remove(MachineID oldSharer) {};
    virtual void clear() {};
    virtual bool isBroadcast() { return false; };
    NetDest n;
    virtual NetDest getSharers() { return n; };
    bool isElement(MachineID id) { return getSharers().isElement(id); }
    int count() { return getSharers().count(); }
    virtual void print(std::ostream& out) const = 0;
};

class SharersImprecise
{
  public:
    SharersImprecise();
    SharersImprecise(bool create) {};
    ~SharersImprecise() {};
    void add(MachineID newSharer) { s->add(newSharer); }
    void remove(MachineID oldSharer) { s->remove(oldSharer); }
    void clear() { s->clear(); }
    bool isBroadcast() { return s->isBroadcast(); }
    NetDest getSharers() { return s->getSharers(); }
    bool isElement(MachineID id) { return s->getSharers().isElement(id); }
    int count() { return s->getSharers().count(); }
    void print(std::ostream& out) const { s->print(out); }
  private:
    AbstractSharersImprecise *s;
};

inline std::ostream&
operator<<(std::ostream& out, const SharersImprecise& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

} // namespace ruby
} // namespace gem5


#endif // __MEM_RUBY_COMMON_SHARERSIMPRECISE_HH__
