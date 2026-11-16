/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Shell Group
 *
 * A cursor-positioning escape sequence (CSI row;col H) puts the cursor
 * where the test asked; tsf-shell reports that cell. Several targets are
 * driven, each in its own session, and the reported column is charted
 * over the targets.
 */

#define TE_TEST_NAME    "shell/cursor"

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
    /* CSI uses 1-based row;col; the reported cell is 0-based. */
    static const struct {
        int row1, col1;   /* what the escape says (1-based) */
        int exp_x, exp_y; /* what tsf-shell should report (0-based) */
    } targets[] = {
        {  1,  1,  0,  0 },
        {  5, 10,  9,  4 },
        { 12, 20, 19, 11 },
        { 24, 80, 79, 23 },
    };

    TEST_START;
    TEST_GET_STRING_PARAM(backend);

    TEST_STEP("Open a session; skip when the agent lacks the backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_cursor"));
    if (!tsapi_shell_sess_select(&sess, backend))
        TEST_SKIP("The agent does not have this terminal-emulator backend");

    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));

    TEST_STEP("Each positioning target is reported at its cell");
    for (i = 0; i < TE_ARRAY_LEN(targets); i++)
    {
        tapi_shell_session sh;
        tapi_shell_status  status;
        te_string          script = TE_STRING_INIT;
        int                cx = -1, cy = -1;
        bool               vis = false;

        te_string_append(&script, "printf '\\033[2J\\033[%d;%dH'",
                         targets[i].row1, targets[i].col1);
        CHECK_RC(tsapi_shell_open_sh(&sess, script.ptr, 80, 24, &sh));
        te_string_free(&script);

        CHECK_RC(tapi_shell_wait(&sh, 5000, &status));
        rc = tapi_shell_cursor(&sh, &cx, &cy, &vis);
        if (rc != 0)
        {
            tapi_shell_close(&sh);
            CHECK_RC(rc);
        }
        RING("target (%d,%d) -> cursor (%d,%d) visible=%d", targets[i].row1,
             targets[i].col1, cx, cy, (int)vis);

        if (cx != targets[i].exp_x || cy != targets[i].exp_y)
        {
            tapi_shell_close(&sh);
            TEST_VERDICT("CSI %d;%dH put the cursor at (%d,%d), expected "
                         "(%d,%d)", targets[i].row1, targets[i].col1, cx, cy,
                         targets[i].exp_x, targets[i].exp_y);
        }

        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "target", TE_MI_MEAS_AGGR_SINGLE, i,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "cursor_col", TE_MI_MEAS_AGGR_SINGLE, cx,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "cursor_row", TE_MI_MEAS_AGGR_SINGLE, cy,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        tapi_shell_close(&sh);
    }

    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "cur", "Reported cursor cell over targets");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "cur",
                               TE_MI_GRAPH_AXIS_X, "target");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "cur", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "cursor_col");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "cur", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "cursor_row");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    tsapi_shell_sess_fini(&sess);
    TEST_END;
}
