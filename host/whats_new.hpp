// Launcher "What's new" page (FU Controls). Opens by itself once per version
// (launcher.ini last_seen_version) and from the home sidebar. The notes come
// from this build's GitHub release (fetched by the updater, cached in
// <data>\whatsnew-cache.txt), else WHATSNEW.md beside the program.
#pragma once

namespace portablekit::fu {
// True when this version's notes have not been shown yet (GameProfile::launcher_whats_new_due).
bool whats_new_due();
// The page. Marks this version as seen. False: the window was closed.
bool show_whats_new_page();
} // namespace portablekit::fu
