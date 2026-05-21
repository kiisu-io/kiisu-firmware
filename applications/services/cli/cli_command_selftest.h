#pragma once

#include <toolbox/pipe.h>

void cli_command_selftest(PipeSide* pipe, FuriString* args, void* context);

/* PC-driven production tests. Each prints a single-line, parseable result.
 * Both block for `args` seconds (default 10) or until success. */
void cli_command_selftest_buttons(PipeSide* pipe, FuriString* args, void* context);
void cli_command_selftest_nfc_card(PipeSide* pipe, FuriString* args, void* context);
