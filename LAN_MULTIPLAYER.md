# LAN multiplayer

Use the updated Freedom Unbound launcher with the same supported MHFU USA game
version on each PC. Each player keeps their own character and save.

## What the existing ad hoc settings do

Ad hoc play switches on the game's virtual wireless connection. **Server** is the
address of the service that introduces players and carries their game traffic.
**Nickname** is the launcher's network name; it does not rename your hunter.
Everyone must connect to the same server and enter the same Online Gathering
Hall number. Entering an address alone does not start a server or enter a Hall.

This launcher already contains a server, discovery and packet/stream relay.
An arbitrary public PSP server may support introductions without supporting this
launcher's traffic relay, so an address accepting a connection does not establish
cross-emulator compatibility. Internet play and other emulators are not verified
by the LAN checks described here.

## Play on the same network

1. Connect all PCs to the same home network. Ethernet and Wi-Fi can be mixed.
2. On one PC, open **Settings > Network > Host a session**. Keep that launcher
   running for the whole session. It shows the local address friends can use.
3. On each other PC, open **Settings > Network** and select that host under
   **Join a session**. If it does not appear, enter the host's local address in
   **Address** and confirm it. Hosting/joining enables ad hoc play automatically.
4. Start the game on every PC and enter **Online Gathering Hall**, choosing the
   same Hall number. The Offline Gathering Hall is single-player.
5. Meet in the Hall, post a quest and join it through the game's normal controls.

The Host and Join controls work before starting the game and in the running
game's Network menu. Selecting a host before boot prepares the connection; the
game connects when it initializes its online features.

Windows may ask whether to allow the launcher through the firewall. Allow it on
your trusted **Private** network. There is no need to forward router ports for
players on the same LAN. The launcher does not change firewall rules for you.

## If players cannot find each other

- Keep the host launcher open, and check that all players chose the same host
  and Hall number. Stopping hosting disconnects its players.
- Use the address shown by the host, not `127.0.0.1`: that address means the
  player's own PC. Try entering the host address if discovery is unavailable.
- Avoid guest Wi-Fi or access-point isolation, which can prevent devices on the
  same Wi-Fi from talking to each other.
- Check the Private-network firewall permission on the host PC.
- Give each installation its own network identity. Copying another installation's
  `network.mac` setting can make the server treat two PCs as the same player.
- The in-game Network page has connection status and **Save network log** for
  diagnosing a failure. Being connected to a server is different from entering
  a Hall or completing a multiplayer quest.

Default services are TCP 27312 (player introduction), TCP 27313 (game traffic),
UDP 27314 (discovery) and UDP 27312 (direct discovery queries). A custom host port
uses that TCP port and the following TCP port, plus discovery. These details are
only needed when managing a firewall manually.

## Implementation and validation

The launcher now exposes the existing Host/Join controls in pre-game settings,
adds short LAN instructions, reports a connection only after the client is
online, and advertises the measured USA game ID. The game also supplied that
same ID during the native Online Hall connection trace.

The Windows networking regression suite passed local discovery, server/client
connections, relayed datagrams and streams, disconnects and process shutdown.
An isolated native MHFU run logged in, joined Hall 01 (`MHP2Q000`) and opened its
gameplay packet socket. Its copied save hashes were unchanged.

Two isolated game instances with separate network identities joined the same
Online Hall through this PC's LAN-interface address. Both listed the other
player, exchanged gameplay packets, and displayed movement made in the other
instance. Both copied saves remained unchanged. This test used copies of the
same tutorial-complete character, not two independently progressed hunters.

Pre-game Host/Join controls were visually checked: hosting started before the
game booted and displayed the local address. Discovery also heard a host on a
second physical PC on the LAN; that session was not joined or modified.

A completed hunt on two physical PCs is not yet verified. Internet play,
cross-emulator play, rewards and multiplayer save/reload need separate tests.
