/** @file
 * @brief Shell Group
 *
 * A scrolling region (DECSTBM) confines vertical scrolling to a band of
 * rows: lines fed past the bottom of the band scroll the band only, and
 * rows outside it stay put. The region is set, enough lines are printed
 * to force one scroll, and the resulting rows are read back.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "shell/scroll"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/*
 * Keep row 3 ("KEEP") outside the region. Region is rows 1..3 (1-based).
 * Print L1,L2,L3 then one more newline at the bottom -> the band scrolls
 * up by one: row0 <- L2, row1 <- L3, row2 blank; row3 ("KEEP") untouched.
 */
#define SCROLL_SCRIPT \
    "printf '\\033[2J\\033[4;1HKEEP\\033[1;3r\\033[1;1HL1\\nL2\\nL3\\n'"

int
main(int argc, char **argv)
{
    tsapi_shell_sess   sess;
    tapi_shell_session sh;
    tapi_shell_status  status;
    te_mi_logger      *logger = NULL;
    bool               opened = false;
    int                y;
    static const struct {
        int         row;
        const char *text;
    } expect[] = {
        { 0, "L2" },
        { 1, "L3" },
        { 2, "" },
        { 3, "KEEP" },
    };

    TEST_START;

    TEST_STEP("Open a session; skip when no backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_scroll"));
    if (!tapi_shell_available(sess.pco))
        TEST_SKIP("The agent has no terminal-emulator backend");

    TEST_STEP("Set a scroll region and force one scroll inside it");
    CHECK_RC(tsapi_shell_open_sh(&sess, SCROLL_SCRIPT, 80, 24, &sh));
    opened = true;
    CHECK_RC(tapi_shell_wait(&sh, 5000, &status));
    if (!status.exited || status.exit_code != 0)
        TEST_VERDICT("the script did not exit cleanly (exited=%d code=%d)",
                     status.exited, status.exit_code);

    TEST_STEP("The band scrolled and the row outside it stayed");
    for (y = 0; y < (int)TE_ARRAY_LEN(expect); y++)
    {
        te_string row = TE_STRING_INIT;

        CHECK_RC(tapi_shell_row_text(&sh, expect[y].row, &row));
        RING("row %d: '%s'", expect[y].row, row.ptr != NULL ? row.ptr : "");
        if (row.ptr == NULL || strcmp(row.ptr, expect[y].text) != 0)
            TEST_VERDICT("row %d is '%s', expected '%s'", expect[y].row,
                         row.ptr != NULL ? row.ptr : "", expect[y].text);
        te_string_free(&row);
    }

    TEST_STEP("Chart the characters present on each of the first rows");
    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    for (y = 0; y < (int)TE_ARRAY_LEN(expect); y++)
    {
        te_string row = TE_STRING_INIT;
        int       n;

        CHECK_RC(tapi_shell_row_text(&sh, y, &row));
        n = row.ptr != NULL ? (int)strlen(row.ptr) : 0;
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "row", TE_MI_MEAS_AGGR_SINGLE, y,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "chars", TE_MI_MEAS_AGGR_SINGLE, n,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_string_free(&row);
    }
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "scroll", "Characters per row after the scroll");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "scroll",
                               TE_MI_GRAPH_AXIS_X, "row");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "scroll", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "chars");
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
