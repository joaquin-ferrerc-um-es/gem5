#ifndef __MEM_RUBY_COMMON_SHARERSJFC_HH__
#define __MEM_RUBY_COMMON_SHARERSJFC_HH__

#include "mem/ruby/common/NetDest.hh"

namespace gem5
{

namespace ruby
{


#define MACHINETYPE MachineType_L1Cache
#define NUMNODES MachineType_base_count(MACHINETYPE)
#define JFCREPRESENTATION RubySystem::getJFCRepresentation()

// Sharers Set
#define MAXPOINTERS RubySystem::getLP()

// Sharers CBV
#define BITSPERPOINTER 8
#define NODESPERGROUP (NUMNODES / (BITSPERPOINTER*MAXPOINTERS))

// Sharers Dasc
#define NUMROWS RubySystem::getNetworkRows()
#define NUMCOLUMNS (NUMNODES/NUMROWS)

enum Direction
{
  UP, DOWN, LEFT, RIGHT
};

enum JFCRepresentation
{
  LP, CBV, DASC
};

enum TypeRepresentation
{
    Representation_Pointers,
    Representation_Broadcast
};

enum TypeRepresentationCBV
{
    Representation_PointersCBV,
    Representation_CoarseBitVector
};

class SharersJFC
{
  public:
    SharersJFC();
    ~SharersJFC() {};
    void add(MachineID newSharer);
    void remove(MachineID oldSharer);
    void clear();
    bool isBroadcast();
    NetDest getSharers();
    void resize();
    void print(std::ostream& out) const;
  private:
    JFCRepresentation JFCRep;

    // Sharers Set
    Set sharers;
    TypeRepresentation type;

    // Sharers CBV
    // Set sharers;           Also in Sharers Set
    std::vector<Set> pointers;
    TypeRepresentationCBV typeCBV;
    void addToCBV(NodeID newSharer);
    void representationToCBV();
    Set getSetSharersCBV();

    // Sharers Dasc
    int getDistanceToHome(MachineID newSharer);
    void addSharers(int node, int distance, Direction direction, Set *sh);
    Set getSetSharersDasc();
    MachineID home;
    int maxDistance;
};

inline std::ostream&
operator<<(std::ostream& out, const SharersJFC& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

} // namespace ruby
} // namespace gem5


#endif // __MEM_RUBY_COMMON_SHARERSJFC_HH__
