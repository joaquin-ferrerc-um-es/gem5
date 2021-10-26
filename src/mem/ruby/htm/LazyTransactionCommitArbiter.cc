/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#include "mem/ruby/htm/LazyTransactionCommitArbiter.hh"

#include "debug/RubyHTM.hh"
#include "debug/RubyHTMverbose.hh"
#include "mem/packet.hh"
#include "mem/ruby/htm/TransactionInterfaceManager.hh"
#include "mem/ruby/system/RubySystem.hh"

#define CLASS_NS LazyTransactionCommitArbiter::

namespace gem5
{
namespace ruby
{

CLASS_NS
LazyTransactionCommitArbiter(TransactionInterfaceManager *xact_mgr,
                             int version,
                             string arbitration_policy) {
    m_version = version;
    m_xact_mgr = xact_mgr;
    m_validated = false;
    m_validating = false;
    m_policy = arbitration_policy;
}

CLASS_NS ~LazyTransactionCommitArbiter() {
}

void
CLASS_NS beginTransaction()
{
    assert(!m_validated);
    assert(!m_validating);
}

void
CLASS_NS restartTransaction(){
    m_validated = false;
    m_validating = false;
}

void
CLASS_NS commitTransaction(){
    assert(m_validated);
    assert(!m_validating);
    m_validated = false;
    m_validating = false;
}


bool
CLASS_NS shouldValidateTransaction()
{
    if (m_policy == HtmPolicyStrings::magic) {
        return true;
    } else {
        panic("Invalid lazy commit validation policy\n");
    }
}

void
CLASS_NS initiateValidateTransaction()
{
    assert(!m_validated);
    m_validating = true;
    if (m_policy == HtmPolicyStrings::magic) {
        // Magic conflict detection at commit time
        std::vector<TransactionInterfaceManager*> mgrs =
            m_xact_mgr->getRemoteTransactionManagers();

        TransactionInterfaceManager* new_committer = m_xact_mgr;
        for (int i=0; i < mgrs.size(); i++) {
            TransactionInterfaceManager* ongoing_committer=mgrs[i];
            if (ongoing_committer->
                getXactLazyCommitArbiter()->validated()) {
                // Already validated committer (guaranteed commit)
                if (ongoing_committer->hasConflictWith(new_committer) ||
                    new_committer->hasConflictWith(ongoing_committer)) {
                    // If this transaction has a conflict with an
                    // ongoing committer, it cannot validate: wait
                    // until ongoing committer ends, may be aborted as
                    // a result of committer invalidations
                    DPRINTF(RubyHTM, "PROC %d validation failed due to "
                            "conflict with proc %d\n", m_version, i);
                    return;
                }
            }
        }
        m_validating = false;
        m_validated = true;
    } else {
        panic("initiateValidateTransaction not tested!\n");
    }
}

} // namespace ruby
} // namespace gem5
