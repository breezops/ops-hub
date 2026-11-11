#include "coop/PartyNpcRoster.h"

#include <algorithm>
#include <functional>
#include <iterator>

namespace coop {
namespace {
bool SortedUniqueIds(const std::vector<uint64_t>& ids) {
    return (ids.empty() || ids.front()!=0) &&
        std::adjacent_find(ids.begin(),ids.end(),std::greater_equal<uint64_t>{})==ids.end();
}
}

bool PartyNpcRoster::RefreshScope() {
    PartyNpcScope scope;
    const bool active=PartyNpcScope::FromRoom(m_Room.Status(),scope);
    if (scope!=m_Scope) {
        m_Scope=scope;
        m_Reports={};
        m_HostReport={};
        m_Local={};
        m_Broadcast={};
        m_Shared.reset();
    }
    return active;
}

void PartyNpcRoster::Replace(Outgoing& outgoing, std::vector<uint64_t> ids) {
    if (outgoing.ids==ids) return;
    outgoing.ids=std::move(ids);
    ++outgoing.revision;
    outgoing.sent=false;
}

void PartyNpcRoster::Send(Outgoing& outgoing,PartyChannel channel) {
    if (outgoing.sent || !outgoing.ids || !outgoing.revision) return;
    const auto& ids=*outgoing.ids;
    const auto pageCount=std::max<size_t>(1,(ids.size()+IdsPerPage-1)/IdsPerPage);
    for (size_t page=0;page<pageCount;++page) {
        ByteWriter writer;
        writer.U8(Packet);
        writer.U32(outgoing.revision);
        writer.U8(static_cast<uint8_t>(page));
        writer.U8(static_cast<uint8_t>(pageCount));
        const auto offset=page*IdsPerPage;
        for (size_t i=offset;i<std::min(ids.size(),offset+IdsPerPage);++i) writer.U64(ids[i]);
        if (!m_Room.SendGame(channel,writer.Data(),true)) return;
    }
    outgoing.sent=true;
}

void PartyNpcRoster::Update(std::span<const uint64_t> localIds) {
    if (!RefreshScope()) return;
    std::vector<uint64_t> ids(localIds.begin(),localIds.end());
    if (ids.size()>MaxIds || !SortedUniqueIds(ids)) ids.clear();
    Replace(m_Local,std::move(ids));
    UpdateShared();
}

void PartyNpcRoster::Tick() {
    if (!RefreshScope()) return;
    if (m_Room.Status().host) {
        if (m_Shared) Replace(m_Broadcast,*m_Shared);
        else if (m_Broadcast.ids) Replace(m_Broadcast,{});
        Send(m_Broadcast,PartyChannel::HostWorld);
    } else {
        Send(m_Local,PartyChannel::Interaction);
    }
}

void PartyNpcRoster::UpdateShared() {
    m_Shared.reset();
    if (!m_Local.ids) return;
    if (!m_Room.Status().host) {
        const auto& shared=m_HostReport.ids;
        if (shared && std::includes(m_Local.ids->begin(),m_Local.ids->end(),shared->begin(),shared->end()))
            m_Shared=shared;
        return;
    }
    auto ids=*m_Local.ids;
    for (size_t seat=1;seat<m_Scope.incarnations.size();++seat) {
        if (!m_Scope.incarnations[seat]) continue;
        const auto& report=m_Reports[seat].ids;
        if (!report) return;
        std::vector<uint64_t> intersection;
        std::set_intersection(ids.begin(),ids.end(),report->begin(),report->end(),std::back_inserter(intersection));
        ids=std::move(intersection);
    }
    m_Shared=std::move(ids);
}

bool PartyNpcRoster::Receive(const PartyGameEvent& event) {
    if (!RefreshScope() || !event.reliable || event.epoch!=m_Scope.epoch ||
        !m_Scope.Contains({event.source,event.incarnation})) return false;
    const bool host=m_Room.Status().host;
    if (host ? (event.channel!=PartyChannel::Interaction || event.source==1) :
               (event.channel!=PartyChannel::HostWorld || event.source!=1)) return false;
    ByteReader reader(event.payload);
    uint8_t packet=0,page=0,count=0;
    uint32_t revision=0;
    if (!reader.U8(packet) || packet!=Packet || !reader.U32(revision) || !revision ||
        !reader.U8(page) || !reader.U8(count) || !count || count>MaxPages || page>=count ||
        reader.Remaining()%sizeof(uint64_t)!=0) return false;
    const auto length=reader.Remaining()/sizeof(uint64_t);
    if (length>IdsPerPage || (page+1<count && length!=IdsPerPage) ||
        (count>1 && !length) || page*IdsPerPage+length>MaxIds) return false;
    std::vector<uint64_t> ids(length);
    for (auto& id:ids) if (!reader.U64(id)) return false;
    if (!SortedUniqueIds(ids)) return false;

    auto& snapshot=host ? m_Reports[event.source-1] : m_HostReport;
    if (revision<snapshot.revision) return false;
    if (revision>snapshot.revision) {
        snapshot={};
        snapshot.revision=revision;
        snapshot.pageCount=count;
        UpdateShared();
    }
    if (snapshot.pageCount!=count) return false;
    if (snapshot.pages[page]) return snapshot.pages[page]==ids;
    snapshot.pages[page]=std::move(ids);
    std::vector<uint64_t> combined;
    for (size_t i=0;i<count;++i) {
        if (!snapshot.pages[i]) return true;
        combined.insert(combined.end(),snapshot.pages[i]->begin(),snapshot.pages[i]->end());
    }
    if (!SortedUniqueIds(combined)) return false;
    snapshot.ids=std::move(combined);
    UpdateShared();
    return true;
}
}
