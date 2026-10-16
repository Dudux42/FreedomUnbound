// host.txt value rule (portablekit/host/ui/host_text.hpp value_text). Header-only, no game data.
#include "ui/host_text.hpp"

#include <cstdio>
#include <cstdlib>

using portablekit::ui::host_text::value_text;

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

int main() {
    CHECK(value_text("\", \"") == ", ");                // quoted separator keeps its space
    CHECK(value_text("\"  /  \"") == "  /  ");
    CHECK(value_text("  \"  /  \"  ") == "  /  ");      // spaces around the quotes are dropped
    CHECK(value_text("\"\"") == "");                    // quoted empty value
    CHECK(value_text(", ") == ", ");                    // unquoted: kept as written
    CHECK(value_text(" Salvar") == " Salvar");
    CHECK(value_text("Diga \"sim\"") == "Diga \"sim\"");  // quotes inside the text stay
    CHECK(value_text("\"Back up\", then play") == "\"Back up\", then play");
    CHECK(value_text("\"") == "\"");                    // a lone quote is text
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}