#pragma once
// Text mods in the game: swaps DATA.BIN string tables after the game has read,
// decrypted and verified them itself (this code never decrypts anything).
#include <cstdint>

namespace psprecomp { class AotFastView; struct AllegrexContext; }

namespace portablekit::fu::text {

// Reads text\NNNN.txt and font\map.txt from the enabled text mods. Once, at startup.
void load_enabled_text_mods();
// 0x0884E648, DATA.BIN worker: after read/decrypt/verify, before "done".
void entry_loaded(psprecomp::AllegrexContext &, const psprecomp::AotFastView &);
// 0x088165FC entry, code point -> glyph code. True if it set v0 and the
// caller should return (goto the original epilogue).
bool map_glyph(psprecomp::AllegrexContext &, const psprecomp::AotFastView &);
// 0x08893198 entry (glyph cache lookup, a0 = text object, a1 = glyph code):
// pins mod code points to their font\map.txt cells before the game looks.
void glyph_cache_lookup(psprecomp::AllegrexContext &, const psprecomp::AotFastView &);
// GameProfile::original_text: English for a translated string, else shown.
const char *original_text(const char *shown);

} // namespace portablekit::fu::text
