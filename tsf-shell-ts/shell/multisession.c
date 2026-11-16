/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Shell Group
 *
 * Several terminal sessions live at once on one agent, each with its
 * own screen, and they do not bleed into one another. Four sessions are
 * opened, each painting a distinct marker; every session is then found
 * to show only its own marker and none of the others.
 */

#define TE_TEST_NAME    "shell/multisession"

#include "te_config.h"

#include <stdio.h>

#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

#define NSESS 4

int
main(int argc, char **argv)
{
    tsapi_shell_sess   sess;
    const char        *backend;
    te_mi_logger      *logger = NULL;
    tapi_shell_session sh[NSESS];
    char               marker[NSESS][16];
    int                opened = 0;
    int                i, j;

    TEST_START;
    TEST_GET_STRING_PARAM(backend);

    TEST_STEP("Open a session; skip when the agent lacks the backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_multi"));
    if (!tsapi_shell_sess_select(&sess, backend))
        TEST_SKIP("The agent does not have this terminal-emulator backend");

    TEST_STEP("Open four sessions, each painting its own marker");
    for (i = 0; i < NSESS; i++)
    {
        tapi_shell_status status;
        te_string         script = TE_STRING_INIT;

        snprintf(marker[i], sizeof(marker[i]), "SESS%d", i);
        te_string_append(&script, "printf '\\033[2J\\033[1;1HSESS%d'", i);
        CHECK_RC(tsapi_shell_open_sh(&sess, script.ptr, 80, 24, &sh[i]));
        te_string_free(&script);
        opened++;
        CHECK_RC(tapi_shell_wait(&sh[i], 5000, &status));
        if (!status.exited || status.exit_code != 0)
            TEST_VERDICT("session %d painter did not exit cleanly "
                         "(exited=%d code=%d)", i, status.exited,
                         status.exit_code);
    }

    TEST_STEP("Each session shows only its own marker");
    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    for (i = 0; i < NSESS; i++)
    {
        if (!tapi_shell_screen_contains(&sh[i], marker[i]))
            TEST_VERDICT("session %d does not show its marker '%s'", i,
                         marker[i]);
        for (j = 0; j < NSESS; j++)
        {
            if (j == i)
                continue;
            if (tapi_shell_screen_contains(&sh[i], marker[j]))
                TEST_VERDICT("session %d leaked marker '%s' from session %d",
                             i, marker[j], j);
        }
        RING("session %d shows only '%s'", i, marker[i]);

        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "session", TE_MI_MEAS_AGGR_SINGLE, i,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "own_markers", TE_MI_MEAS_AGGR_SINGLE, 1,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
    }
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "multi", "Own markers visible per session");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "multi",
                               TE_MI_GRAPH_AXIS_X, "session");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "multi", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "own_markers");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    for (i = 0; i < opened; i++)
        tapi_shell_close(&sh[i]);
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    tsapi_shell_sess_fini(&sess);
    TEST_END;
}
