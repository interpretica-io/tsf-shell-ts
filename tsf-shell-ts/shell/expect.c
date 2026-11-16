/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Shell Group
 *
 * An interactive shell is driven: input is typed in, the echo is waited
 * for on the output stream (classic expect), and the same text is then
 * found on the rendered screen with its cell coordinates. The
 * round-trip latency is charted over the payloads.
 */

#define TE_TEST_NAME    "shell/expect"

#include "te_config.h"

#include <time.h>

#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/* Read a line and echo it back with a prefix; loop forever. */
#define ECHO_SCRIPT \
    "while IFS= read -r line; do echo \"got:$line\"; done"

static int64_t
now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int
main(int argc, char **argv)
{
    tsapi_shell_sess   sess;
    const char        *backend;
    tapi_shell_session sh;
    te_mi_logger      *logger = NULL;
    bool               opened = false;
    unsigned int       i;
    static const char * const payloads[] = { "alpha", "bravo", "charlie" };

    TEST_START;
    TEST_GET_STRING_PARAM(backend);

    TEST_STEP("Open a session; skip when the agent lacks the backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_expect"));
    if (!tsapi_shell_sess_select(&sess, backend))
        TEST_SKIP("The agent does not have this terminal-emulator backend");

    TEST_STEP("Spawn an interactive echo shell");
    CHECK_RC(tsapi_shell_open_sh(&sess, ECHO_SCRIPT, 80, 24, &sh));
    opened = true;

    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));

    TEST_STEP("Type each payload and wait for its echo on the stream");
    for (i = 0; i < TE_ARRAY_LEN(payloads); i++)
    {
        te_string want = TE_STRING_INIT;
        int64_t   t0, dt;

        te_string_append(&want, "got:%s", payloads[i]);

        t0 = now_ms();
        CHECK_RC(tapi_shell_send(&sh, payloads[i]));
        CHECK_RC(tapi_shell_send_key(&sh, TAPI_SHELL_KEY_ENTER, 0));

        rc = tapi_shell_expect_str(&sh, want.ptr, 5000);
        dt = now_ms() - t0;
        if (rc != 0)
        {
            te_string_free(&want);
            TEST_VERDICT("the echo of '%s' did not arrive (%r)", payloads[i],
                         rc);
        }
        RING("'%s' echoed in %d ms", payloads[i], (int)dt);

        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "payload", TE_MI_MEAS_AGGR_SINGLE, i,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "rtt_ms", TE_MI_MEAS_AGGR_SINGLE, (double)dt,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_string_free(&want);
    }

    TEST_STEP("The last echo is also found on the rendered screen");
    {
        tapi_shell_expect_opts opts = {
            .where = TAPI_SHELL_WHERE_SCREEN,
            .timeout_ms = 2000,
        };
        tapi_shell_match match = { 0 };

        rc = tapi_shell_expect(&sh, "got:charlie", &opts, &match);
        if (rc != 0)
            TEST_VERDICT("'got:charlie' not found on the screen (%r)", rc);
        RING("screen match at cell (%d,%d): '%s'", match.x, match.y,
             match.text != NULL ? match.text : "");
        if (match.x < 0 || match.y < 0)
            TEST_VERDICT("screen match has no cell coordinates (%d,%d)",
                         match.x, match.y);
        tapi_shell_match_free(&match);
    }

    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "lat", "Echo round-trip latency (ms)");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "lat",
                               TE_MI_GRAPH_AXIS_X, "payload");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "lat", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "rtt_ms");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    if (opened)
        tapi_shell_close(&sh);
    tsapi_shell_sess_fini(&sess);
    TEST_END;
}
