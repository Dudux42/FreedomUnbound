# This disc

Read from the USA image on this machine. The image itself is not in this repository.

- ISO9660 volume label: MHFU
- Image size: 886,702,080 bytes
- PARAM.SFO DISC_ID: ULUS10391
- PARAM.SFO TITLE: MONSTER HUNTER FREEDOM UNITE™
- PARAM.SFO DISC_VERSION: 1.01
- PARAM.SFO PSP_SYSTEM_VER: 5.03
- PARAM.SFO CATEGORY: UG

## Encrypted executable

PSP_GAME/SYSDIR/EBOOT.BIN, as stored on the disc. 1,910,672 bytes.

- SHA-256: 3354adb3ee40821f230c4055b9bf7cc5d4c864e42abb74ce8ac022933f9697a7
- Header: ~PSP, file type 9 (UMD game executable), attribute word 0x0002 (the compressed bit is clear)
- Image size at header offset 0x28: 1,910,332 (0x1D263C)
- Payload size at header offset 0xB0: 1,910,332 (0x1D263C)
- Tag at offset 0xD0: 0xC0CB167C

PortableKit does not publish a 16-byte key or a 0x90-byte table for tag 0xC0CB167C. crypto_keys_builtin.cpp has the engine keys and the save-data keys, and no tag entries. DESKTOP_APP.md only shows the name tag.C0CB167C as a keys-file example, and says the test keys file was kept outside the repository. EBOOT.BIN was not decrypted. There is no SHA-256 of a decrypted ELF.

## Unencrypted executable on the same disc

PSP_GAME/SYSDIR/BOOT.BIN is a plain ELF, 1,910,332 bytes, the same length as the image size in the EBOOT header. The installer now reads this file directly, and its measured hash is the profile's executable_sha256. This does not establish byte-for-byte equality with a decrypted EBOOT.

- SHA-256: 3070954e2dc88fd0b0836c0c1dc526531aca4a213b0f18806e324963efb514f4
- ELF type: 2 (ET_EXEC), machine 8. Not a PRX (type is not 0xFFA0).
- Entry: 0x08906A54
- One PT_LOAD: vaddr 0x08804000, filesz 0x001C7BB0, memsz 0x01561580, flags 0x7
- That image ends at guest 0x09D65580, which fits in 32 MiB, so guest RAM is 0x02000000
- Program-header count is 300. The remaining 299 headers are type 0; their addresses and maximum memory extents describe task, subtask, monster, stage and debug slots. Zero-sized named overlay sections corroborate the layout. The section named .bss is at 0x089CBBB0 with size 0. The profile now supplies the measured slot boundaries; see INVESTIGATION.md for the layout and runtime confirmation of two boot overlays.

Save strings in BOOT.BIN, verbatim, with file offsets:

- 0x198848: ms0:\.\PSP\SAVEDATA\ULUS10391DAT\%08d
- 0x19888C: ms0:\.\PSP\SAVEDATA\ULUS10391DAT
- 0x1AFD94, 0x1B0F14, 0x1B348C: ULUS10391
- 0x1B34A8: ULUS10391DAT

Also present, and not recorded as save folders: ULUS10266 at 0x1B35A4, next to MHP2ND.BIN; MHP2NDG.BIN at 0x1B3498; fatms0: at 0x1B34C4.

These identifiers name the player's own disc. This file is not a copy of the game and it is not permission to redistribute it.
