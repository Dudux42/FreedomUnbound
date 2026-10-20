// Launcher update button: checks the Freedom Unbound GitHub releases (prereleases
// included) and, when a newer one exists, downloads its ZIP, verifies the
// published .sha256, and replaces only the program files. User data (settings,
// mods, saves) lives under %APPDATA% and is never touched. Nothing downloads
// until the player presses the button.
#pragma once
#include <string>

namespace portablekit::fu {
// Shared numeric/prerelease ordering for launcher and mod replacement checks.
bool version_is_newer(const std::string &candidate, const std::string &installed);
// Draws the bottom-left update button. True: an update was staged and the
// launcher must quit so the helper can swap the files and restart it. When up
// to date, offline or failed, pressing it checks again ("Check for updates").
bool draw_update_button(float width);

// Non-blocking banner across the top of the home screen when a newer release
// exists and the player has not dismissed that version: "Open release page"
// opens the browser, "Dismiss for this version" writes update_dismissed=<tag>
// to launcher.ini. Never downloads anything. Draws nothing otherwise.
void draw_update_banner(float x, float y, float width);

// This build's notes from its GitHub release, once a check has fetched them
// this session (FU Controls, "What's new"). False until then or if none.
bool current_release_notes(std::string &tag, std::string &notes);
// Starts the release check if nothing has started it yet (the button does
// the same on its first draw). Never downloads.
void ensure_update_check();
}
