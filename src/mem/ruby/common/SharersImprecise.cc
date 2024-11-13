#include "mem/ruby/common/SharersImprecise.hh"
#include "mem/ruby/common/LPBroadcast.hh"
#include "mem/ruby/common/LPCBV.hh"
#include "mem/ruby/common/DASC.hh"

namespace gem5
{

namespace ruby
{

SharersImprecise::SharersImprecise()
{
  if (RubySystem::getImpreciseRepresentation() == "lp") {
    s = new LPBroadcast();
  }
  else if (RubySystem::getImpreciseRepresentation() == "coarse_bit_vector") {
    s = new LPCBV();
  }
  else if (RubySystem::getImpreciseRepresentation() == "dasc") {
    s = new DASC();
  }
  else {
    assert(false);
  }
}

}
}
