/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Shell Group
 *
 * A spawned program's exit status is reported back: several exit codes
 * are checked, and a process that kills itself with a signal is
 * reported as signalled. The exit codes are charted over the spawns.
 */

#define TE_TEST_NAME    "shell/wait"

#include "te_config.h"

#include <signal.h>

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
    static const int codes[] = { 0, 3, 7, 42 };

    TEST_START;
    TEST_GET_STRING_PARAM(backend);

    TEST_STEP("Open a session; skip when the agent lacks the backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_wait"));
    if (!tsapi_shell_sess_select(&sess, backend))
        TEST_SKIP("The agent does not have this terminal-emulator backend");

    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));

    TEST_STEP("Each spawned program's exit code is reported");
    for (i = 0; i < TE_ARRAY_LEN(codes); i++)
    {
        tapi_shell_session sh;
        tapi_shell_status  status;
        te_string          script = TE_STRING_INIT;

        te_string_append(&script, "exit %d", codes[i]);
        CHECK_RC(tsapi_shell_open_sh(&sess, script.ptr, 80, 24, &sh));
        te_string_free(&script);

        rc = tapi_shell_wait(&sh, 5000, &status);
        if (rc != 0)
        {
            tapi_shell_close(&sh);
            TEST_VERDICT("wait for 'exit %d' failed: %r", codes[i], rc);
        }
        RING("exit %d -> exited=%d code=%d signal=%d", codes[i], status.exited,
             status.exit_code, status.exit_signal);
        if (!status.exited || status.exit_code != codes[i])
        {
            tapi_shell_close(&sh);
            TEST_VERDICT("'exit %d' reported exited=%d code=%d", codes[i],
                         status.exited, status.exit_code);
        }

        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "spawn", TE_MI_MEAS_AGGR_SINGLE, i,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "exit_code", TE_MI_MEAS_AGGR_SINGLE,
                              status.exit_code, TE_MI_MEAS_MULTIPLIER_PLAIN);
        tapi_shell_close(&sh);
    }

    TEST_STEP("A process killed by a signal is reported as signalled");
    {
        tapi_shell_session sh;
        tapi_shell_status  status;

        CHECK_RC(tsapi_shell_open_sh(&sess, "kill -TERM $$; sleep 5", 80, 24,
                                     &sh));
        rc = tapi_shell_wait(&sh, 5000, &status);
        if (rc != 0)
        {
            tapi_shell_close(&sh);
            TEST_VERDICT("wait for the self-kill failed: %r", rc);
        }
        RING("self-kill -> exited=%d code=%d signal=%d", status.exited,
             status.exit_code, status.exit_signal);
        if (!status.exited || status.exit_signal != SIGTERM)
        {
            tapi_shell_close(&sh);
            TEST_VERDICT("self-kill reported exited=%d signal=%d, expected "
                         "signal %d", status.exited, status.exit_signal,
                         SIGTERM);
        }
        tapi_shell_close(&sh);
    }

    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "exit", "Reported exit code per spawn");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "exit",
                               TE_MI_GRAPH_AXIS_X, "spawn");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "exit", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "exit_code");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    tsapi_shell_sess_fini(&sess);
    TEST_END;
}
