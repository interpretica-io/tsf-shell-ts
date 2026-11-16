/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Shell Group
 *
 * A symbolic key is turned into the byte sequence an xterm-style
 * terminal sends, so a test never hand-writes escape sequences. The
 * child is put in raw mode and dumps the exact bytes it receives; each
 * key (and an Alt-modified key) is sent and its byte sequence asserted
 * on the output stream.
 */

#define TE_TEST_NAME    "shell/keys"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

int
main(int argc, char **argv)
{
    tsapi_shell_sess sess;
    const char        *backend;
    te_mi_logger    *logger = NULL;
    unsigned int     i;
    static const struct {
        tapi_shell_key key;
        unsigned int   mods;
        int            nbytes;
        const char    *hex;    /* od -An -tx1 substring */
        const char    *label;
    } cases[] = {
        { TAPI_SHELL_KEY_ENTER,     0, 1, "0d",          "Enter" },
        { TAPI_SHELL_KEY_TAB,       0, 1, "09",          "Tab" },
        { TAPI_SHELL_KEY_BACKSPACE, 0, 1, "7f",          "Backspace" },
        { TAPI_SHELL_KEY_UP,        0, 3, "1b 5b 41",    "Up" },
        { TAPI_SHELL_KEY_DOWN,      0, 3, "1b 5b 42",    "Down" },
        { TAPI_SHELL_KEY_RIGHT,     0, 3, "1b 5b 43",    "Right" },
        { TAPI_SHELL_KEY_LEFT,      0, 3, "1b 5b 44",    "Left" },
        { TAPI_SHELL_KEY_HOME,      0, 3, "1b 5b 48",    "Home" },
        { TAPI_SHELL_KEY_F1,        0, 3, "1b 4f 50",    "F1" },
        { TAPI_SHELL_KEY_DELETE,    0, 4, "1b 5b 33 7e", "Delete" },
        { TAPI_SHELL_KEY_PGUP,      0, 4, "1b 5b 35 7e", "PageUp" },
        { TAPI_SHELL_KEY_UP, TAPI_SHELL_MOD_ALT, 4, "1b 1b 5b 41", "Alt-Up" },
    };

    TEST_START;
    TEST_GET_STRING_PARAM(backend);

    TEST_STEP("Open a session; skip when the agent lacks the backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_keys"));
    if (!tsapi_shell_sess_select(&sess, backend))
        TEST_SKIP("The agent does not have this terminal-emulator backend");

    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));

    TEST_STEP("Each key is received as its exact byte sequence");
    for (i = 0; i < TE_ARRAY_LEN(cases); i++)
    {
        tapi_shell_session sh;
        te_string          script = TE_STRING_INIT;

        /*
         * Raw mode, announce readiness (RDY), then dump exactly nbytes
         * received as hex and exit. The RDY handshake closes the race
         * between our send and the child reaching raw mode: a key sent
         * while the tty is still cooked would be mangled by the line
         * discipline. od prints lowercase hex, so RDY never collides.
         */
        te_string_append(&script,
                         "stty raw -echo; printf RDY; head -c%d | od -An -tx1",
                         cases[i].nbytes);
        CHECK_RC(tsapi_shell_open_sh(&sess, script.ptr, 80, 24, &sh));
        te_string_free(&script);

        rc = tapi_shell_expect_str(&sh, "RDY", 5000);
        if (rc != 0)
        {
            tapi_shell_close(&sh);
            TEST_VERDICT("the reader did not reach raw mode for '%s' (%r)",
                         cases[i].label, rc);
        }

        CHECK_RC(tapi_shell_send_key(&sh, cases[i].key, cases[i].mods));

        rc = tapi_shell_expect_str(&sh, cases[i].hex, 5000);
        if (rc != 0)
        {
            tapi_shell_close(&sh);
            TEST_VERDICT("key '%s' did not arrive as '%s' (%r)",
                         cases[i].label, cases[i].hex, rc);
        }
        RING("key '%s' -> '%s'", cases[i].label, cases[i].hex);

        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "key", TE_MI_MEAS_AGGR_SINGLE, i,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "bytes", TE_MI_MEAS_AGGR_SINGLE, cases[i].nbytes,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        tapi_shell_close(&sh);
    }

    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "keys", "Byte length of each key sequence");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "keys",
                               TE_MI_GRAPH_AXIS_X, "key");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "keys", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "bytes");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    tsapi_shell_sess_fini(&sess);
    TEST_END;
}
