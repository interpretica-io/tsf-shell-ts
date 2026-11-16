/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Shell Group
 *
 * Save-cursor (DECSC) and restore-cursor (DECRC) bracket a detour: the
 * cursor is parked, moved elsewhere to print, then restored to the
 * parked spot to print again. The two printed cells prove the cursor
 * went where DECRC said and not where the detour left it.
 */

#define TE_TEST_NAME    "shell/savecursor"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/*
 * Move to row 5 col 10 (0-based 4,9), DECSC. Home, print "TOP". DECRC
 * back to (9,4), print "X".
 */
#define SAVE_SCRIPT \
    "printf '\\033[2J\\033[5;10H\\0337\\033[1;1HTOP\\0338X'"

int
main(int argc, char **argv)
{
    tsapi_shell_sess   sess;
    const char        *backend;
    tapi_shell_session sh;
    tapi_shell_status  status;
    te_mi_logger      *logger = NULL;
    bool               opened = false;
    te_string          row0 = TE_STRING_INIT;
    tapi_shell_cell_t  c;

    TEST_START;
    TEST_GET_STRING_PARAM(backend);

    TEST_STEP("Open a session; skip when the agent lacks the backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_savecursor"));
    if (!tsapi_shell_sess_select(&sess, backend))
        TEST_SKIP("The agent does not have this terminal-emulator backend");

    TEST_STEP("Park, detour to print, then restore and print");
    CHECK_RC(tsapi_shell_open_sh(&sess, SAVE_SCRIPT, 80, 24, &sh));
    opened = true;
    CHECK_RC(tapi_shell_wait(&sh, 5000, &status));
    if (!status.exited || status.exit_code != 0)
        TEST_VERDICT("the script did not exit cleanly (exited=%d code=%d)",
                     status.exited, status.exit_code);

    TEST_STEP("The detour wrote 'TOP' at the home row");
    CHECK_RC(tapi_shell_row_text(&sh, 0, &row0));
    RING("row 0: '%s'", row0.ptr != NULL ? row0.ptr : "");
    if (row0.ptr == NULL || strcmp(row0.ptr, "TOP") != 0)
        TEST_VERDICT("row 0 is '%s', expected 'TOP'",
                     row0.ptr != NULL ? row0.ptr : "");

    TEST_STEP("The restored cursor wrote 'X' back at the parked cell");
    CHECK_RC(tapi_shell_cell(&sh, 9, 4, &c));
    RING("cell(9,4) cp=U+%04X", c.codepoint);
    if (c.codepoint != (uint32_t)'X')
        TEST_VERDICT("cell(9,4) is U+%04X, expected 'X'", c.codepoint);

    TEST_STEP("Chart the row the restored cursor wrote on");
    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "event", TE_MI_MEAS_AGGR_SINGLE, 0,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "restored_row", TE_MI_MEAS_AGGR_SINGLE, 4,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "event", TE_MI_MEAS_AGGR_SINGLE, 1,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "restored_row", TE_MI_MEAS_AGGR_SINGLE, 4,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "save", "Row written after DECRC");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "save",
                               TE_MI_GRAPH_AXIS_X, "event");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "save", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "restored_row");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    te_string_free(&row0);
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    if (opened)
        tapi_shell_close(&sh);
    tsapi_shell_sess_fini(&sess);
    TEST_END;
}
