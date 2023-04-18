#ifndef __MEM_RUBY_COMMON_SHARERSDASH_HH__
#define __MEM_RUBY_COMMON_SHARERSDASH_HH__

#include "mem/ruby/common/NetDest.hh"
#include "mem/ruby/common/SharersJFC.hh"

namespace gem5
{

namespace ruby
{

/*enum Direction
{
  UP, DOWN, LEFT, RIGHT
};*/

class SharersDash
{
  public:
    SharersDash();
    ~SharersDash() {};
    void add(MachineID newSharer);
    void remove(MachineID oldSharer);
    void clear();
    bool isBroadcast();
    NetDest getSharers();
    void print(std::ostream& out) const;
  private:
    int getDistanceToHome(MachineID newSharer);
    void addSharers(int node, int distance, Direction direction, Set *sharers);
    Set getSetSharers();
    MachineID home;
    int maxDistance;
};

inline std::ostream&
operator<<(std::ostream& out, const SharersDash& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

} // namespace ruby
} // namespace gem5


#endif // __MEM_RUBY_COMMON_SHARERSDASH_HH__
