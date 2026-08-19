/* GBAVitaEX — src/core/rfu_vita_net.c
 * PSVita UDP transport for gpSP RFU wireless-adapter multiplayer.
 *
 * Architecture
 * ────────────
 * - Single UDP socket, non-blocking (SCE_NET_MSG_DONTWAIT on recv).
 * - Host uses 255.255.255.255:RFU_PORT for discovery broadcasts.
 *   After discovery, unicast to each known peer IP.
 * - Clients broadcast a JOIN packet; host replies with their assigned id.
 * - netpacket_send / netpacket_poll_receive are called synchronously from
 *   gpSP's main emulation thread (rfu_frame_update once per vblank).
 *   No extra threads needed.
 *
 * Limitations
 * ───────────
 * - LAN only (same subnet). No internet relay.
 * - Max 4 clients (hardware RFU limit from gpSP, MAX_RFU_NETPLAYERS=32 but
 *   the RFU protocol itself caps at 4 clients + 1 host = 5 total).
 * - Router must allow UDP broadcasts (most home routers do).
 */

#include "rfu_vita_net.h"

/* Pull in gpSP RFU types/callbacks */
#include "retro_inline.h"
#include "common.h"
#include "serial.h"   /* rfu_net_receive, serialpoke_net_receive, etc. */

#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ──────────────────────────────────────────────────────────────────────────
   Wire-format helpers
   ────────────────────────────────────────────────────────────────────────── */
#define HDR_SIZE 4   /* 2-byte sender + 2-byte dest */

typedef struct {
    uint16_t sender_id;
    uint16_t dest_id;
    uint8_t  payload[RFU_MAX_PACKET];
    size_t   payload_len;
} RfuPacket;

/* ──────────────────────────────────────────────────────────────────────────
   State
   ────────────────────────────────────────────────────────────────────────── */
#define MAX_PEERS 5

static bool     s_active      = false;
static bool     s_is_host     = false;
static int      s_sock        = -1;
static uint8_t  s_net_mem[1 * 1024 * 1024];   /* SceNet work buffer: 1 MB */
static char     s_status[128] = "Idle";

typedef struct { SceNetSockaddrIn addr; bool valid; } Peer;
static Peer s_peers[MAX_PEERS];   /* index = client_id */

/* Discovery packet magic */
#define MAGIC_JOIN    0xCAFE0001u
#define MAGIC_ASSIGN  0xCAFE0002u

/* ── gpSP globals we need to read/write — defined here, declared extern
 * in the gpSP common.h → serial.h chain that serial.c/serial_proto.c use.
 * They were previously no-op stubs in stubs.c; now they carry real state. ── */
u32 netplay_client_id   = 0;
u32 netplay_num_clients = 0;

/* ──────────────────────────────────────────────────────────────────────────
   Utilities
   ────────────────────────────────────────────────────────────────────────── */
static void make_bcast_addr(SceNetSockaddrIn *a) {
    memset(a, 0, sizeof(*a));
    a->sin_family = SCE_NET_AF_INET;
    a->sin_port   = sceNetHtons(RFU_PORT);
    a->sin_addr.s_addr = sceNetHtonl(SCE_NET_INADDR_BROADCAST);
}

/* ──────────────────────────────────────────────────────────────────────────
   Public: start / stop
   ────────────────────────────────────────────────────────────────────────── */
bool rfu_vita_net_start(bool host) {
    if (s_active) return true;

    /* SceNet init */
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceNetCtlInit();

    SceNetInitParam np = { .memory = s_net_mem, .size = sizeof(s_net_mem), .flags = 0 };
    if (sceNetInit(&np) < 0) {
        snprintf(s_status, sizeof(s_status), "sceNetInit failed");
        return false;
    }

    /* Open non-blocking UDP socket */
    s_sock = sceNetSocket("rfu_vita", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
    if (s_sock < 0) {
        snprintf(s_status, sizeof(s_status), "socket open failed: 0x%08X", s_sock);
        sceNetTerm();
        return false;
    }

    /* Allow broadcast sends */
    int yes = 1;
    sceNetSetsockopt(s_sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_BROADCAST, &yes, sizeof(yes));
    /* Non-blocking receive */
    sceNetSetsockopt(s_sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &yes, sizeof(yes));

    /* Bind to RFU_PORT so we can receive */
    SceNetSockaddrIn bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family     = SCE_NET_AF_INET;
    bind_addr.sin_port       = sceNetHtons(RFU_PORT);
    bind_addr.sin_addr.s_addr = sceNetHtonl(SCE_NET_INADDR_ANY);
    if (sceNetBind(s_sock, (SceNetSockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        snprintf(s_status, sizeof(s_status), "bind failed");
        sceNetSocketClose(s_sock); s_sock = -1;
        sceNetTerm();
        return false;
    }

    s_is_host = host;
    memset(s_peers, 0, sizeof(s_peers));

    if (host) {
        /* Host is always client_id 0 */
        netplay_client_id  = 0;
        netplay_num_clients = 0;
        snprintf(s_status, sizeof(s_status), "Host — waiting for clients");
    } else {
        /* Send JOIN broadcast, wait for host assignment */
        uint8_t join[8];
        uint32_t magic = MAGIC_JOIN;
        memcpy(join, &magic, 4);
        memset(join+4, 0, 4);
        SceNetSockaddrIn bcast;
        make_bcast_addr(&bcast);
        sceNetSendto(s_sock, join, sizeof(join), 0,
                     (SceNetSockaddr *)&bcast, sizeof(bcast));
        snprintf(s_status, sizeof(s_status), "Client — sent JOIN, waiting...");
    }

    s_active = true;
    return true;
}

void rfu_vita_net_stop(void) {
    if (!s_active) return;
    sceNetSocketClose(s_sock); s_sock = -1;
    sceNetTerm();
    s_active           = false;
    netplay_client_id  = 0;
    netplay_num_clients = 0;
    snprintf(s_status, sizeof(s_status), "Idle");
}

bool        rfu_vita_net_active(void) { return s_active; }
const char *rfu_vita_net_status(void) { return s_status; }

/* ──────────────────────────────────────────────────────────────────────────
   netpacket_send — called by gpSP's rfu.c / serial_proto.c
   client_id == RETRO_NETPACKET_BROADCAST (0xFFFF) → send to all peers
   client_id == specific id → unicast to that peer
   ────────────────────────────────────────────────────────────────────────── */
void netpacket_send(uint16_t client_id, const void *buf, size_t len) {
    if (!s_active || s_sock < 0 || len == 0 || len > RFU_MAX_PACKET) return;

    /* Build wire packet: [sender_id:2][dest_id:2][payload:N] */
    uint8_t wire[HDR_SIZE + RFU_MAX_PACKET];
    wire[0] = (netplay_client_id >> 8) & 0xFF;
    wire[1] =  netplay_client_id       & 0xFF;
    wire[2] = (client_id >> 8) & 0xFF;
    wire[3] =  client_id       & 0xFF;
    memcpy(wire + HDR_SIZE, buf, len);
    size_t total = HDR_SIZE + len;

    if (client_id == 0xFFFF) {
        /* Broadcast to all known peers */
        SceNetSockaddrIn bcast;
        make_bcast_addr(&bcast);
        sceNetSendto(s_sock, wire, (unsigned)total, 0,
                     (SceNetSockaddr *)&bcast, sizeof(bcast));
    } else {
        /* Unicast — host uses stored peer address, clients unicast to host */
        if (s_is_host && client_id < MAX_PEERS && s_peers[client_id].valid) {
            sceNetSendto(s_sock, wire, (unsigned)total, 0,
                         (SceNetSockaddr *)&s_peers[client_id].addr,
                         sizeof(s_peers[client_id].addr));
        } else if (!s_is_host && s_peers[0].valid) {
            /* Client → host only */
            sceNetSendto(s_sock, wire, (unsigned)total, 0,
                         (SceNetSockaddr *)&s_peers[0].addr,
                         sizeof(s_peers[0].addr));
        }
    }
}

/* ──────────────────────────────────────────────────────────────────────────
   netpacket_poll_receive — called synchronously by rfu_frame_update()
   Reads all pending UDP datagrams from the socket and dispatches them to
   the correct gpSP receive callback.
   ────────────────────────────────────────────────────────────────────────── */
void netpacket_poll_receive(void) {
    if (!s_active || s_sock < 0) return;

    uint8_t wire[HDR_SIZE + RFU_MAX_PACKET];
    SceNetSockaddrIn from;
    unsigned from_len = sizeof(from);

    for (;;) {
        int recv = sceNetRecvfrom(s_sock, wire, sizeof(wire),
                                  SCE_NET_MSG_DONTWAIT,
                                  (SceNetSockaddr *)&from, &from_len);
        if (recv <= HDR_SIZE) break;   /* no data or header-only → done */

        uint16_t sender_id = ((uint16_t)wire[0] << 8) | wire[1];
        uint16_t dest_id   = ((uint16_t)wire[2] << 8) | wire[3];
        const uint8_t *payload = wire + HDR_SIZE;
        size_t  plen = (size_t)(recv - HDR_SIZE);

        /* Host: handle JOIN discovery and assign client IDs */
        if (s_is_host && plen >= 4) {
            uint32_t magic;
            memcpy(&magic, payload, 4);
            if (magic == MAGIC_JOIN) {
                /* Find or assign a slot */
                int slot = -1;
                for (int i = 1; i < MAX_PEERS; i++) {
                    if (!s_peers[i].valid) { slot = i; break; }
                    /* Already known? (same IP+port) */
                    if (s_peers[i].addr.sin_addr.s_addr == from.sin_addr.s_addr &&
                        s_peers[i].addr.sin_port == from.sin_port) {
                        slot = i; break;
                    }
                }
                if (slot > 0) {
                    s_peers[slot].addr  = from;
                    s_peers[slot].valid = true;
                    netplay_num_clients = slot;

                    /* Reply ASSIGN with their new client_id */
                    uint8_t assign[8];
                    uint32_t amag = MAGIC_ASSIGN;
                    memcpy(assign, &amag, 4);
                    assign[4] = (uint8_t)slot;
                    assign[5] = assign[6] = assign[7] = 0;
                    sceNetSendto(s_sock, assign, sizeof(assign), 0,
                                 (SceNetSockaddr *)&from, sizeof(from));
                    snprintf(s_status, sizeof(s_status),
                             "Host — %d client(s) connected", (int)netplay_num_clients);
                }
                continue;
            }
        }

        /* Client: handle ASSIGN from host */
        if (!s_is_host && plen >= 5) {
            uint32_t magic;
            memcpy(&magic, payload, 4);
            if (magic == MAGIC_ASSIGN) {
                uint8_t assigned_id = payload[4];
                netplay_client_id = assigned_id;
                s_peers[0].addr   = from;
                s_peers[0].valid  = true;
                snprintf(s_status, sizeof(s_status),
                         "Client — connected as player %d", (int)assigned_id);
                continue;
            }
        }

        /* Ignore packets not addressed to us */
        if (dest_id != 0xFFFF && dest_id != netplay_client_id) continue;

        /* Route to the right gpSP receive callback based on serial_mode */
        switch (serial_mode) {
        case SERIAL_MODE_RFU:
            rfu_net_receive(payload, plen, sender_id);
            break;
        case SERIAL_MODE_SERIAL_POKE:
            serialpoke_net_receive(payload, plen, sender_id);
            break;
        case SERIAL_MODE_SERIAL_AW1:
        case SERIAL_MODE_SERIAL_AW2:
            serialaw_net_receive(payload, plen, sender_id);
            break;
        default:
            break;
        }
    }
}
