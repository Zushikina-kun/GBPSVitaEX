/* GBVitaEX — src/core/rfu_vita_net.h
 * PSVita UDP transport for gpSP's RFU wireless-adapter multiplayer.
 *
 * gpSP's RFU/serial layer calls two stubs that we implement here:
 *   void netpacket_send(uint16_t client_id, const void *buf, size_t len)
 *   void netpacket_poll_receive(void)
 *
 * These replace the no-op stubs in stubs.c when rfu_vita_net_start() has
 * been called.  We use a simple broadcast UDP approach on the LAN:
 *   - One Vita is the host   (netplay_client_id == 0, listens on RFU_PORT)
 *   - Others are clients     (netplay_client_id  > 0, auto-assigned by host)
 *
 * Wire protocol (all big-endian):
 *   [2 bytes] sender client_id
 *   [2 bytes] dest   client_id  (0xFFFF = broadcast)
 *   [N bytes] payload
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define RFU_PORT          7354          /* arbitrary LAN port */
#define RFU_MAX_PACKET    512           /* max RFU payload bytes */
#define RFU_DISCOVERY_MS  2000          /* peer discovery window (ms) */

/* Initialise SceNet (idempotent) and open the UDP socket.
 * host=true  → this Vita is the game host (client_id=0)
 * host=false → this Vita is a client; waits for host to assign an id */
bool rfu_vita_net_start(bool host);

/* Shut down the socket. */
void rfu_vita_net_stop(void);

/* Returns true if the network is currently active. */
bool rfu_vita_net_active(void);

/* Returns a human-readable status string for the UI. */
const char *rfu_vita_net_status(void);
