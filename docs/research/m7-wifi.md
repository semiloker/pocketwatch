# M7 — Wi-Fi in Pocketwatch Home, and why the network never came up

Question: why did every network job in Home fail (`sceNetInit` 0x8002013A), and how can the
owner connect the PSP to a network from Home - pick it, type its password - without the XMB?
Status: Answered (2026-10-20): the cause found and fixed in the build; the connection screen is
implemented, its device test (HM33) pending.

## Background and sources

- The owner's round-7 report (2026-10-20): "Wi-Fi does not work on the PSP, it says not available;
  and if it worked, how would I connect - there is no section, I cannot choose or type a password".
  Raw log: [`logs/2026-10-20-round7-home.log`](logs/2026-10-20-round7-home.log).
- The log: every Set Time from Internet ends at `switch=1 profile=0` - no connection saved in the
  PSP at all (VERIFIED); the browser: `switch=0 ... init=0x8002013A step=0`, and once after it
  `switch=0x8002013A` - `sceWlanGetSwitchState` itself unlinked after the browser had let its
  libraries go (VERIFIED, log lines 145-146).
- 0x8002013A is `SCE_ERROR_KERNEL_LIBRARY_IS_NOT_LINKED`; an import the loader could not link has
  `syscall 0x15` in its stub, and linking a stub table against a library that lacks one of its NIDs
  leaves that stub so (uOFW `src/kd/loadcore/loadcore.c`, `ReleaseLibEntCB`, `aLinkClient`;
  VERIFIED from the source).
- `psp-fixup-imports` (pspdev/pspsdk `tools/psp-fixup-imports.c`, `fixup_imports`) builds each
  library's import table from the stubs that name it: the first stub starts the table and every
  later one adds one to its count - so the stubs of one library must be together; else it warns
  "could not fixup imports, stubs out of order" and the table covers whatever follows its first
  stub (VERIFIED from the source).
- PSP Wi-Fi: 802.11b, 2.4 GHz only; WEP, WPA-PSK (TKIP), WPA-PSK (AES, which WPA2 routers accept);
  no WPA3 (INFERRED from the XMB's own security choices and common router guides; the owner's
  router's settings are not known).
- A connection's parameters (`PSP_NETPARAM_*` 0-24, PPSSPP `Core/HLE/sceUtility.h`; `SECURE` 0
  none, 1-2 WEP, 3 WPA; `WPA_TYPE` 0 hex, 1 passphrase; jpcsp `sceReg.java` has the registry's
  `/CONFIG/NETWORK/INFRASTRUCTURE/<n>` keys behind them). The firmware's own way to make one:
  `sceUtility_netparam_internal` Create / Set / Copy / Delete (PSPSDK
  `src/utility/sceUtility_netparam_internal.S`), which the firmware's `pspnet_ap_dialog_dummy`
  uses - security 3 and 4 for the two WPA kinds (yne/js-psp `reverse/pspnet_ap_dialog_dummy.c`;
  INFERRED from a decompilation).
- The search: `sceNetApctlScanUser`, `GetBSSDescIDListUser`, `GetBSSDescEntryUser` (NIDs: PPSSPP
  `Core/HLE/sceNet.cpp`); an entry's SSID (1), its length (2), channel (3), signal 0-100 (4) and
  security (5: 0 none, 1 WEP, 2 WPA, 3 unsupported, 4 WPA-AES - PPSSPP `sceNetApctl.h`).

## Procedure

Host: the stub tables of `home/pocketwatch_home.elf` listed by library (name, count, NIDs, the
functions at their stubs); the link done again without the fixup and `psp-fixup-imports -v` run on
it; the link map read for who pulled each `sceNetInet` stub. PPSSPP 1.19.3 with WLAN on: Settings
→ Wi-Fi Connections → Search for Networks, Enter on a network. Device: test HM33.

## Results

**Host, 2026-10-20.** Home's `sceNetInet` table claimed 10 functions where Home's code imports 7;
its last three entries were the stubs of `sceNetInit`, `sceNetTerm` and `sceWlanGetSwitchState`
(their NIDs in its NID list). The fixup's trace: 7 `sceNetInet` stubs in the libraries' block, 3
more after `sceCtrl`'s; the link map: newlib's glue (`libcglue.a`: `recv.o`, `send.o`,
`__socket_close.o`) pulled `sceNetInetRecv`, `sceNetInetSend`, `sceNetInetGetErrno` at the end
of the link. The build log had the "stubs out of order" warning in every build since the browser
(615e6d5 too: its table also had 10). So: loading `NET_COMMON` linked `sceNetInit`; loading
`NET_INET` linked `sceNetInet`'s 10 entries, found no `sceNetInit` in that library and left the
stub unlinked - the 0x8002013A of the log, and the unlinked switch after the browser.

A first try to add the search's three functions with an `IMPORT_START "sceNetApctl"` of our own
made it worse (VERIFIED): it took the SDK header's symbol, and the table took sceUsb's and
sceHttp's stubs (`tools/check_prx.py` now fails on a stub claimed by two tables).

**PPSSPP, 2026-10-20.** With the three `sceNetInet` stubs taken with the others (`-Wl,-u`) and the
search's three as `IMPORT_FUNC`s linked right after `-lpspnet_apctl`, the tables are whole (no
warning; `sceNetInet` 10 of its own, `sceNetApctl` 9). Search for Networks: "4 found" (PPSSPP's
fake WifiAP0-3, strongest first, signal bars); Enter on an open one: `home net: join -6 ... 
netparam=0x8002013A` - PPSSPP has no netparam calls (as expected), Home says it could not save.

## Conclusions

- VERIFIED: the network never came up in Home because of its own build (the import table), not
  the firmware or ARK-4; fixed (home/Makefile), and `make check` fails on it now.
- The PSP had no saved connection: Home now makes one (ADR-044, HOM-REQ-033). Not verified on a
  device: that the netparam calls are open to a user program (a kernel-mode fallback is there), the
  security values 3 / 4 and the key type 1 for a passphrase (a test of a connection made in the
  XMB logs `secure=` and `wpa_type=` to compare), the search's entries on the real firmware.
- Next: HM33; if the router is WPA3-only or 5 GHz-only the search will not show it - the page says
  which networks the PSP can see.
