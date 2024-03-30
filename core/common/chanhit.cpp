// ------------------------------------------------
// File : chanhit.cpp
// Date: 4-apr-2002
// Author: giles
//
// (c) 2002 peercast.org
//
// ------------------------------------------------
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// ------------------------------------------------

#include "chanhit.h"

#include "pcp.h"
#include "servmgr.h"
#include "version2.h"

// -----------------------------------
ChanHitList::ChanHitList()
    : used(false)
    , hit(nullptr)
    , lastHitTime(0)
    , next(nullptr)
{
}

// -----------------------------------
ChanHitList::~ChanHitList()
{
    while (hit)
        hit = deleteHit(hit);
}

// -----------------------------------
void ChanHit::pickNearestIP(Host &h)
{
    for (int i = 0; i < 2; i++)
    {
        if (h.classType() == rhost[i].classType())
        {
            host = rhost[i];
            break;
        }
    }
}

// -----------------------------------
void ChanHit::init()
{
    host.init();

    rhost[0].init();
    rhost[1].init();

    next = nullptr;

    numListeners = 0;
    numRelays = 0;

    dead = tracker = firewalled = stable = yp = false;
    recv = cin = direct = relay = true;

    direct = 0;
    numHops = 0;
    time = upTime = 0;
    lastContact = 0;

    version = 0;

    sessionID.clear();
    chanID.clear();

    oldestPos = newestPos = 0;

    uphost.init();
    uphostHops = 0;

    versionVP = 0;
    memcpy(versionExPrefix, "  ", 2);
    versionExNumber = 0;
}

// -----------------------------------
void ChanHit::initLocal(
    int numl,
    int numr,
    int,
    int uptm,
    bool connected,
    unsigned int oldp,
    unsigned int newp,
    bool canAddRelay,
    const Host& sourceHost,
    bool ipv6)
{
    std::lock_guard<std::recursive_mutex> cs(servMgr->lock);

    init();

    firewalled = (servMgr->getFirewall(ipv6 ? 6 : 4) != ServMgr::FW_OFF);
    numListeners = numl;
    numRelays = numr;
    upTime = uptm;
    stable = servMgr->totalStreams>0;
    sessionID = servMgr->sessionID;
    recv = connected;

    direct = !servMgr->directFull();
    relay = canAddRelay;
    cin = !servMgr->controlInFull();

    if (ipv6)
        host = servMgr->serverHostIPv6;
    else
        host = servMgr->serverHost;

    version = PCP_CLIENT_VERSION;
    versionVP = PCP_CLIENT_VERSION_VP;
    memcpy(versionExPrefix, PCP_CLIENT_VERSION_EX_PREFIX, 2);
    versionExNumber = PCP_CLIENT_VERSION_EX_NUMBER;

    rhost[0] = host;
    if (ipv6)
        rhost[1] = Host(servMgr->serverLocalIPv6, host.port);
    else
        rhost[1] = Host(servMgr->serverLocalIP, host.port);

    if (firewalled)
        rhost[0].port = 0;

    oldestPos = oldp;
    newestPos = newp;

    uphost      = sourceHost;
    uphostHops  = 1;
}

// -----------------------------------
static int flags1(ChanHit* hit)
{
    int fl1 = 0;
    if (hit->recv)       fl1 |= PCP_HOST_FLAGS1_RECV;
    if (hit->relay)      fl1 |= PCP_HOST_FLAGS1_RELAY;
    if (hit->direct)     fl1 |= PCP_HOST_FLAGS1_DIRECT;
    if (hit->cin)        fl1 |= PCP_HOST_FLAGS1_CIN;
    if (hit->tracker)    fl1 |= PCP_HOST_FLAGS1_TRACKER;
    if (hit->firewalled) fl1 |= PCP_HOST_FLAGS1_PUSH;
    return fl1;
}

// -----------------------------------
void ChanHit::writeAtoms(AtomStream &atom, const GnuID &chanID)
{
    bool addChan = chanID.isSet();

    atom.writeParent(PCP_HOST,
                     13  +
                     (addChan ? 1 : 0) +
                     (uphost.ip ? 3 : 0) +
                     (versionExNumber != 0 ? 2 : 0));
        if (addChan)
            atom.writeBytes(PCP_HOST_CHANID, chanID.id, 16);
        atom.writeBytes(PCP_HOST_ID, sessionID.id, 16);
        atom.writeAddress(PCP_HOST_IP, rhost[0].ip);
        atom.writeShort(PCP_HOST_PORT, rhost[0].port);
        atom.writeAddress(PCP_HOST_IP, rhost[1].ip);
        atom.writeShort(PCP_HOST_PORT, rhost[1].port);
        atom.writeInt(PCP_HOST_NUML, numListeners);
        atom.writeInt(PCP_HOST_NUMR, numRelays);
        atom.writeInt(PCP_HOST_UPTIME, upTime);
        atom.writeInt(PCP_HOST_VERSION, version);
        atom.writeInt(PCP_HOST_VERSION_VP, versionVP);
        if (versionExNumber)
        {
            atom.writeBytes(PCP_HOST_VERSION_EX_PREFIX, versionExPrefix, 2);
            atom.writeShort(PCP_HOST_VERSION_EX_NUMBER, versionExNumber);
        }
        atom.writeChar(PCP_HOST_FLAGS1, flags1(this));
        atom.writeInt(PCP_HOST_OLDPOS, oldestPos);
        atom.writeInt(PCP_HOST_NEWPOS, newestPos);
        if (uphost.ip)
        {
            atom.writeAddress(PCP_HOST_UPHOST_IP, uphost.ip);
            atom.writeInt(PCP_HOST_UPHOST_PORT, uphost.port);
            atom.writeInt(PCP_HOST_UPHOST_HOPS, uphostHops);
        }
}

// -----------------------------------
amf0::Value    ChanHit::getState()
{
    std::string ver = versionString();

    return {
        {"rhost0", rhost[0].str()},
        {"rhost1", rhost[1].str()},
        {"numHops", std::to_string(numHops)},
        {"numListeners", std::to_string(numListeners)},
        {"numRelays", std::to_string(numRelays)},
        {"uptime", String().setFromStopwatch(upTime).c_str()},
        {"update", (time) ? String().setFromStopwatch(sys->getTime()-time).c_str() : "-"},
        {"isFirewalled", firewalled ? "1" : "0"},
        {"version", ver.empty() ? std::string("-") : ver},
        {"tracker", std::to_string(tracker)},
    };
}

// -----------------------------------
std::string ChanHit::versionString()
{
    using namespace std;
    if (!version)
        return "";
    else if (!versionVP)
        return to_string(version);
    else if (!versionExNumber)
        return "VP" + to_string(versionVP);
    else
        return string() + versionExPrefix[0] + versionExPrefix[1] + to_string(versionExNumber);
}

// -----------------------------------
// 選択されたホストの情報を簡潔に文字列化する。
std::string ChanHit::str(bool withPort)
{
    auto res = host.str(withPort);

    if (!versionString().empty())
        res += " (" + versionString() + ")";
    return res;
}

// -----------------------------------
// ノードの色。
ChanHit::Color ChanHit::getColor()
{
    if (host.port == 0)
    {
        return Color::red;
    }else if (!relay)
    {
        if (numRelays == 0)
        {
            return Color::purple;
        }else
        {
            return Color::blue;
        }
    }else
    {
        return Color::green;
    }
}

// -----------------------------------
// GIVメソッドに対応している。
bool ChanHit::canGiv()
{
    // PeerCastStation以外。
    return std::string(versionExPrefix, versionExPrefix + 2) != "ST";
}

// -----------------------------------
int ChanHitList::getTotalListeners()
{
    int cnt = 0;
    auto h = hit;
    while (h)
    {
        if (h->host.ip)
            cnt += h->numListeners;
        h = h->next;
    }
    return cnt;
}

// -----------------------------------
int ChanHitList::getTotalRelays()
{
    int cnt = 0;
    auto h = hit;
    while (h)
    {
        if (h->host.ip)
            cnt += h->numRelays;
        h = h->next;
    }
    return cnt;
}

// -----------------------------------
int ChanHitList::getTotalFirewalled()
{
    int cnt = 0;
    auto h = hit;
    while (h)
    {
        if (h->host.ip)
            if (h->firewalled)
                cnt++;
        h = h->next;
    }
    return cnt;
}

// -----------------------------------
int ChanHitList::contactTrackers(bool connected, int numl, int nums, int uptm)
{
    return 0;
}

// -----------------------------------
// ch で参照される項目を削除し、次の項目へのポインタ、あるいは次の項目
// が存在しない場合は nullptr を返す。
std::shared_ptr<ChanHit> ChanHitList::deleteHit(std::shared_ptr<ChanHit> ch)
{
    std::shared_ptr<ChanHit> c = hit, prev = nullptr;
    while (c)
    {
        if (c == ch)
        {
            auto next = c->next;
            if (prev)
                prev->next = next;
            else
                hit = next;

            return next;
        }
        prev = c;
        c = c->next;
    }

    return nullptr;
}

// -----------------------------------
std::shared_ptr<ChanHit> ChanHitList::addHit(ChanHit &h)
{
    LOG_DEBUG("Add hit: %s/%s", h.rhost[0].str().c_str(), h.rhost[1].str().c_str());

    // dont add our own hits
    if (servMgr->sessionID.isSame(h.sessionID))
        return nullptr;

    lastHitTime = sys->getTime();
    h.time = lastHitTime;

    auto ch = hit;
    while (ch)
    {
        if ((ch->rhost[0].ip == h.rhost[0].ip) &&
            (ch->rhost[0].port == h.rhost[0].port))
            if (((ch->rhost[1].ip == h.rhost[1].ip) && (ch->rhost[1].port == h.rhost[1].port)) ||
                (!ch->rhost[1].isValid()))
            {
                if (!ch->dead)
                {
                    auto next = ch->next;
                    *ch = h;
                    ch->next = next;
                    return ch;
                }
            }
        ch = ch->next;
    }

    // clear hits with same session ID (IP may have changed)
    if (h.sessionID.isSet())
    {
        auto ch = hit;
        while (ch)
        {
            if (ch->host.ip)
                if (ch->sessionID.isSame(h.sessionID))
                {
                    ch = deleteHit(ch);
                    continue;
                }
            ch = ch->next;
        }
    }

    // else add new hit
    {
        auto ch = std::make_shared<ChanHit>();
        *ch = h;
        ch->chanID = info.id;
        ch->next = hit;
        hit = ch;
        return ch;
    }

    return nullptr;
}

// -----------------------------------
// 古いヒットを削除し、リストに残ったヒットの数を返す。
int ChanHitList::clearDeadHits(unsigned int timeout, bool clearTrackers)
{
    int cnt = 0;
    unsigned int ctime = sys->getTime();

    auto ch = hit;
    while (ch)
    {
        if (ch->host.ip)
        {
            bool oldEnough = (ctime - ch->time) > timeout;
            if (ch->dead ||
                (oldEnough && (clearTrackers || !ch->tracker)))
            {
                ch = deleteHit(ch);
                continue;
            }else
                cnt++;
        }
        ch = ch->next;
    }
    return cnt;
}

// -----------------------------------
void    ChanHitList::deadHit(ChanHit &h)
{
    LOG_DEBUG("Dead hit: %s/%s", h.rhost[0].str().c_str(), h.rhost[1].str().c_str());

    auto ch = hit;
    while (ch)
    {
        if (ch->host.ip)
            if (ch->rhost[0].isSame(h.rhost[0]) && ch->rhost[1].isSame(h.rhost[1]))
            {
                ch->dead = true;
            }
        ch = ch->next;
    }
}

// -----------------------------------
void    ChanHitList::delHit(ChanHit &h)
{
    LOG_DEBUG("Del hit: %s/%s", h.rhost[0].str().c_str(), h.rhost[1].str().c_str());

    auto ch = hit;
    while (ch)
    {
        if (ch->host.ip)
            if (ch->rhost[0].isSame(h.rhost[0]) && ch->rhost[1].isSame(h.rhost[1]))
            {
                ch = deleteHit(ch);
                continue;
            }
        ch = ch->next;
    }
}

// -----------------------------------
int ChanHitList::numHits()
{
    int cnt = 0;
    auto ch = hit;
    while (ch)
    {
        if (ch->host.ip && !ch->dead)
            cnt++;
        ch = ch->next;
    }

    return cnt;
}

// -----------------------------------
int ChanHitList::numListeners()
{
    int cnt = 0;
    auto ch = hit;
    while (ch)
    {
        if (ch->host.ip && !ch->dead)
            cnt += ch->numListeners;
        ch = ch->next;
    }

    return cnt;
}

// -----------------------------------
int ChanHitList::numRelays()
{
    int cnt = 0;
    auto ch = hit;
    while (ch)
    {
        if (ch->host.ip && !ch->dead)
            cnt += ch->numRelays;
        ch = ch->next;
    }

    return cnt;
}

// -----------------------------------
int ChanHitList::numTrackers()
{
    int cnt = 0;
    auto ch = hit;
    while (ch)
    {
        if ((ch->host.ip && !ch->dead) && (ch->tracker))
            cnt++;
        ch = ch->next;
    }
    return cnt;
}

// -----------------------------------
int ChanHitList::numFirewalled()
{
    int cnt = 0;
    auto ch = hit;
    while (ch)
    {
        if (ch->host.ip && !ch->dead)
            cnt += ch->firewalled?1:0;
        ch = ch->next;
    }
    return cnt;
}

// -----------------------------------
int ChanHitList::closestHit()
{
    unsigned int hop = 10000;
    auto ch = hit;
    while (ch)
    {
        if (ch->host.ip && !ch->dead)
            if (ch->numHops < hop)
                hop = ch->numHops;
        ch = ch->next;
    }

    return hop;
}

// -----------------------------------
int ChanHitList::furthestHit()
{
    unsigned int hop = 0;
    auto ch = hit;
    while (ch)
    {
        if (ch->host.ip && !ch->dead)
            if (ch->numHops > hop)
                hop = ch->numHops;
        ch = ch->next;
    }

    return hop;
}

// -----------------------------------
unsigned int    ChanHitList::newestHit()
{
    unsigned int time = 0;
    auto ch = hit;
    while (ch)
    {
        if (ch->host.ip && !ch->dead)
            if (ch->time > time)
                time = ch->time;
        ch = ch->next;
    }

    return time;
}

// -----------------------------------
void ChanHitList::forEachHit(std::function<void(ChanHit*)> block)
{
    auto chl = shared_from_this();

    while (chl)
    {
        if (chl->isUsed())
        {
            auto hit = chl->hit;
            while (hit)
            {
                block(hit.get());
                hit = hit->next;
            }
        }
        chl = chl->next;
    }
}

// -----------------------------------
int ChanHitList::pickHits(ChanHitSearch &chs)
{
    ChanHit best;
    std::shared_ptr<ChanHit> bestP = nullptr;
    best.init();
    best.numHops = 255;
    best.time = 0;

    unsigned int ctime = sys->getTime();

    auto c = hit;
    while (c)
    {
        if (c->host.ip && !c->dead)
        {
            if (!chs.excludeID.isSame(c->sessionID))
            if ((chs.waitDelay == 0) || ((ctime - c->lastContact) >= chs.waitDelay))
            if (c->numHops < best.numHops)
            if (c->relay || (!c->relay && chs.useBusyRelays))
            if (c->cin || (!c->cin && chs.useBusyControls))
            {
                if (chs.trackersOnly && c->tracker)
                {
                    if (chs.matchHost.ip)
                    {
                        if ((c->rhost[0].ip == chs.matchHost.ip) && c->rhost[1].isValid())
                        {
                            bestP = c;
                            best = *c;
                            best.host = best.rhost[1];  // use lan ip
                        }
                    }else if (c->firewalled == chs.useFirewalled)
                    {
                        bestP = c;
                        best = *c;
                        best.host = best.rhost[0];          // use wan ip
                    }
                }else if (!chs.trackersOnly && !c->tracker)
                {
                    if (chs.matchHost.ip)
                    {
                        if ((c->rhost[0].ip == chs.matchHost.ip) && c->rhost[1].isValid())
                        {
                            bestP = c;
                            best = *c;
                            best.host = best.rhost[1];  // use lan ip
                        }
                    }else if (c->firewalled == chs.useFirewalled)
                    {
                        bestP = c;
                        best = *c;
                        best.host = best.rhost[0];          // use wan ip
                    }
                }
            }
        }
        c = c->next;
    }

    if (bestP)
    {
        if (chs.numResults < ChanHitSearch::MAX_RESULTS)
        {
            if (chs.waitDelay)
                bestP->lastContact = ctime;
            chs.best[chs.numResults++] = best;
            return 1;
        }
    }

    return 0;
}

// -----------------------------------
void ChanHitSearch::init()
{
    matchHost.init();
    waitDelay = 0;
    useFirewalled = false;
    trackersOnly = false;
    useBusyRelays = true;
    useBusyControls = true;
    excludeID.clear();
    numResults = 0;
}
