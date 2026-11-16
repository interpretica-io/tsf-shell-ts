/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Shell Group
 *
 * Filling a row to its last column leaves the terminal in "pending
 * wrap": the cursor sits on the last cell and the next character goes
 * to the next row. Any explicit move to the left cancels that state.
 * Cursor-left (CUB) is the control; cursor-backward-tab (CBT) is the
 * case under test, because an emulator that moves the cursor on CBT
 * but keeps the pending wrap prints the next character one row down
 * from where the cursor is shown.
 */

#define TE_TEST_NAME    "shell/cbtwrap"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/*
 * Four columns. Row 0: "abcd", four cells left, "X"  -> "Xbcd".
 *               Row 2: "efgh", back-tab,        "Y"  -> "Yfgh".
 * If the back-tab keeps the pending wrap, row 2 stays "efgh" and "Y"
 * lands on row 3.
 */
#define CBT_SCRIPT \
    "printf '\\033[2J\\033[1;1Habcd\\033[4DX\\033[3;1Hefgh\\033[ZY'"
#define CBT_COLS    4

int
main(int argc, char **argv)
{
    tsapi_shell_sess   sess;
    const char        *backend;
    tapi_shell_session sh;
    tapi_shell_status  status;
    te_mi_logger      *logger = NULL;
    bool               opened = false;
    te_string          cub = TE_STRING_INIT;
    te_string          cbt = TE_STRING_INIT;
    te_string          below = TE_STRING_INIT;
    int                cub_same_row;
    int                cbt_same_row;

    TEST_START;
    TEST_GET_STRING_PARAM(backend);

    TEST_STEP("Open a session; skip when the agent lacks the backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_cbtwrap"));
    if (!tsapi_shell_sess_select(&sess, backend))
        TEST_SKIP("The agent does not have this terminal-emulator backend");

    TEST_STEP("Fill two rows to the last column, then move left and print");
    CHECK_RC(tsapi_shell_open_sh(&sess, CBT_SCRIPT, CBT_COLS, 24, &sh));
    opened = true;
    CHECK_RC(tapi_shell_wait(&sh, 5000, &status));
    if (!status.exited || status.exit_code != 0)
        TEST_VERDICT("the script did not exit cleanly (exited=%d code=%d)",
                     status.exited, status.exit_code);

    CHECK_RC(tapi_shell_row_text(&sh, 0, &cub));
    CHECK_RC(tapi_shell_row_text(&sh, 2, &cbt));
    CHECK_RC(tapi_shell_row_text(&sh, 3, &below));
    RING("after cursor-left: row 0 '%s'", cub.ptr != NULL ? cub.ptr : "");
    RING("after back-tab:    row 2 '%s', row 3 '%s'",
         cbt.ptr != NULL ? cbt.ptr : "",
         below.ptr != NULL ? below.ptr : "");

    cub_same_row = (cub.ptr != NULL && strcmp(cub.ptr, "Xbcd") == 0);
    cbt_same_row = (cbt.ptr != NULL && strcmp(cbt.ptr, "Yfgh") == 0);

    TEST_STEP("Chart whether the next character stayed on the cursor row");
    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "motion", TE_MI_MEAS_AGGR_SINGLE, 0,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "same_row", TE_MI_MEAS_AGGR_SINGLE, cub_same_row,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "motion", TE_MI_MEAS_AGGR_SINGLE, 1,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "same_row", TE_MI_MEAS_AGGR_SINGLE, cbt_same_row,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "cbtwrap",
                               "Next character on the cursor row "
                               "(0 = cursor-left, 1 = back-tab)");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "cbtwrap",
                               TE_MI_GRAPH_AXIS_X, "motion");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "cbtwrap", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "same_row");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_STEP("Cursor-left cancels the pending wrap (control)");
    if (!cub_same_row)
        TEST_VERDICT("cursor-left kept the pending wrap: row 0 is '%s'",
                     cub.ptr != NULL ? cub.ptr : "");

    TEST_STEP("Back-tab cancels the pending wrap as well");
    if (!cbt_same_row)
    {
        if (below.ptr != NULL && strchr(below.ptr, 'Y') != NULL)
            TEST_VERDICT("back-tab kept the pending wrap: the next "
                         "character went to the row below the cursor");
        TEST_VERDICT("back-tab left row 2 as '%s'",
                     cbt.ptr != NULL ? cbt.ptr : "");
    }

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    te_string_free(&cub);
    te_string_free(&cbt);
    te_string_free(&below);
    if (opened)
        tapi_shell_close(&sh);
    tsapi_shell_sess_fini(&sess);
    TEST_END;
}
