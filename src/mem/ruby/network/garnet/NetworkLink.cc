/*
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
 * Copyright (c) 2020 Inria
 * Copyright (c) 2016 Georgia Institute of Technology
 * Copyright (c) 2008 Princeton University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "mem/ruby/network/garnet/NetworkLink.hh"

#include "base/trace.hh"
#include "mem/ruby/network/Network.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet/CreditLink.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

NetworkLink::NetworkLink(const Params &p)
    : ClockedObject(p), Consumer(this), m_id(p.link_id),
      m_type(NUM_LINK_TYPES_),
      m_latency(p.link_latency), m_link_utilized(0),
      m_virt_nets(p.virt_nets), linkBuffer(),
      link_consumer(nullptr), link_srcQueue(nullptr),
      networkLinkStats(this)
{
    int num_vnets = (p.supported_vnets).size();
    mVnets.resize(num_vnets);
    bitWidth = p.width;
    for (int i = 0; i < num_vnets; i++) {
        mVnets[i] = p.supported_vnets[i];
    }
}

void
NetworkLink::setLinkConsumer(Consumer *consumer)
{
    link_consumer = consumer;
}

void
NetworkLink::setVcsPerVnet(uint32_t consumerVcs)
{
    m_vc_load.resize(m_virt_nets * consumerVcs);
}

void
NetworkLink::setSourceQueue(flitBuffer *src_queue, ClockedObject *srcClockObj)
{
    link_srcQueue = src_queue;
    src_object = srcClockObj;
}

void
NetworkLink::wakeup()
{
    DPRINTF(RubyNetwork, "Woke up to transfer flits from %s\n",
        src_object->name());
    assert(link_srcQueue != nullptr);
    assert(curTick() == clockEdge());
    if (link_srcQueue->isReady(curTick())) {
        flit *t_flit = link_srcQueue->getTopFlit();
        DPRINTF(RubyNetwork, "Transmission will finish at %ld :%s\n",
                clockEdge(m_latency), *t_flit);
        if (m_type != NUM_LINK_TYPES_) {
            // Only for assertions and debug messages
            assert(t_flit->m_width == bitWidth);
            assert((std::find(mVnets.begin(), mVnets.end(),
                t_flit->get_vnet()) != mVnets.end()) ||
                (mVnets.size() == 0));
        }
        t_flit->set_time(clockEdge(m_latency));
        linkBuffer.insert(t_flit);
        if (t_flit->get_type() == TAIL_ || t_flit->get_type() == HEAD_TAIL_) {
            (*(networkLinkStats.m_msg_counts[t_flit->get_msg_ptr()->getMessageSize()]))[t_flit->get_vnet()]++;
        }
        link_consumer->scheduleEventAbsolute(clockEdge(m_latency));
        m_link_utilized++;
        m_vc_load[t_flit->get_vc()]++;
    }

    if (!link_srcQueue->isEmpty()) {
        scheduleEvent(Cycles(1));
    }
}

void
NetworkLink::regStats()
{
    ClockedObject::regStats();

    for (MessageSizeType type = MessageSizeType_FIRST;
         type < MessageSizeType_NUM; ++type) {
        networkLinkStats.m_msg_counts[(unsigned int)type] =
            new statistics::Vector(&networkLinkStats,
            csprintf("msg_count.%s", MessageSizeType_to_string(type)).c_str());
        networkLinkStats.m_msg_counts[(unsigned int)type]
            ->init(Network::getNumberOfVirtualNetworks())
            .flags(statistics::nozero)
            ;

        networkLinkStats.m_msg_bytes[(unsigned int) type] =
            new statistics::Formula(&networkLinkStats,
            csprintf("msg_bytes.%s", MessageSizeType_to_string(type)).c_str());
        networkLinkStats.m_msg_bytes[(unsigned int) type]
            ->flags(statistics::nozero)
            ;

        *(networkLinkStats.m_msg_bytes[(unsigned int) type]) =
            *(networkLinkStats.m_msg_counts[type]) * statistics::constant(
                Network::MessageSizeType_to_int(type));
    }
}

void
NetworkLink::resetStats()
{
    for (int i = 0; i < m_vc_load.size(); i++) {
        m_vc_load[i] = 0;
    }

    m_link_utilized = 0;
}

uint32_t
NetworkLink::functionalWrite(Packet *pkt)
{
    return linkBuffer.functionalWrite(pkt);
}

NetworkLink::
NetworkLinkStats::NetworkLinkStats(statistics::Group *parent)
    : statistics::Group(parent)
{

}

} // namespace garnet
} // namespace ruby
} // namespace gem5
