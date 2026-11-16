/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Shell Group
 *
 * A control character typed into a terminal in its normal (cooked) mode
 * is turned into a signal by the line discipline: Ctrl-C interrupts the
 * foreground program, Ctrl-backslash quits it. A reader is started, its
 * readiness confirmed, the control key sent, and the terminating signal
 * read back from the child's exit status.
 */

#define TE_TEST_NAME    "shell/signals"

#include "te_config.h"

#include <signal.h>

#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/* A plain line reader; no core files when quit. */
#define CAT_SCRIPT "ulimit -c 0 2>/dev/null; exec cat"

int
main(int argc, char **argv)
{
    tsapi_shell_sess sess;
    const char        *backend;
    te_mi_logger    *logger = NULL;
    unsigned int     i;
    static const struct {
        char        ctrl;
        int         want_signal;
        const char *label;
    } cases[] = {
        { 'C',  SIGINT,  "Ctrl-C" },
        { '\\', SIGQUIT, "Ctrl-backslash" },
    };

    TEST_START;
    TEST_GET_STRING_PARAM(backend);

    TEST_STEP("Open a session; skip when the agent lacks the backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_signals"));
    if (!tsapi_shell_sess_select(&sess, backend))
        TEST_SKIP("The agent does not have this terminal-emulator backend");

    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));

    TEST_STEP("Each control key terminates the reader with its signal");
    for (i = 0; i < TE_ARRAY_LEN(cases); i++)
    {
        tapi_shell_session sh;
        tapi_shell_status  status;

        CHECK_RC(tsapi_shell_open_sh(&sess, CAT_SCRIPT, 80, 24, &sh));

        /*
         * Confirm the reader is up before the control key: a typed line
         * comes back twice, once as the terminal's echo and once from
         * cat. The echo alone is not enough - the terminal echoes before
         * the shell has even turned into cat, and a control key sent then
         * signals the shell instead.
         */
        CHECK_RC(tapi_shell_send(&sh, "ping"));
        CHECK_RC(tapi_shell_send_key(&sh, TAPI_SHELL_KEY_ENTER, 0));
        {
            tapi_shell_expect_opts eo = { .regex = true,
                                          .where = TAPI_SHELL_WHERE_STREAM,
                                          .timeout_ms = 5000 };
            tapi_shell_match       m;

            memset(&m, 0, sizeof(m));
            rc = tapi_shell_expect(&sh, "ping[\r\n]+ping", &eo, &m);
            tapi_shell_match_free(&m);
        }
        if (rc != 0)
        {
            tapi_shell_close(&sh);
            TEST_VERDICT("the reader never echoed before '%s' (%r)",
                         cases[i].label, rc);
        }

        CHECK_RC(tapi_shell_send_ctrl(&sh, cases[i].ctrl));

        rc = tapi_shell_wait(&sh, 5000, &status);
        if (rc != 0)
        {
            tapi_shell_close(&sh);
            TEST_VERDICT("wait after '%s' failed: %r", cases[i].label, rc);
        }
        RING("'%s' -> exited=%d code=%d signal=%d", cases[i].label,
             status.exited, status.exit_code, status.exit_signal);
        if (!status.exited || status.exit_signal != cases[i].want_signal)
        {
            tapi_shell_close(&sh);
            TEST_VERDICT("'%s' reported exited=%d signal=%d, expected "
                         "signal %d", cases[i].label, status.exited,
                         status.exit_signal, cases[i].want_signal);
        }

        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "case", TE_MI_MEAS_AGGR_SINGLE, i,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "signal", TE_MI_MEAS_AGGR_SINGLE,
                              status.exit_signal, TE_MI_MEAS_MULTIPLIER_PLAIN);
        tapi_shell_close(&sh);
    }

    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "sig", "Terminating signal per control key");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "sig",
                               TE_MI_GRAPH_AXIS_X, "case");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "sig", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "signal");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    tsapi_shell_sess_fini(&sess);
    TEST_END;
}
