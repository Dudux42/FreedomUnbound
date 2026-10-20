#pragma once

namespace portablekit::fu {

// Re-reads text\host.txt from every enabled text mod (load order) and
// refreshes the host string table. Call after a text mod is toggled or moved.
void reload_host_text();

} // namespace portablekit::fu
