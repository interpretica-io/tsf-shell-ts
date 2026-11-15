/** @file
 * @brief Shell Group
 *
 * Erase-in-line (EL) and erase-in-display clear exactly the cells the
 * escape names and no others. A painted screen has parts of it erased
 * to the end of a line and a whole line cleared; the surviving text is
 * read back and asserted, and the count of non-blank cells per row is
 * charted before and after the erase.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "shell/erase"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/*
 * Row 0: "AAAAA", untouched.
 * Row 1: "BBBBB", then erase from column 3 to the end -> "BB".
 * Row 2: "CCCCC", then erase the whole line -> "".
 */
#define ERASE_SCRIPT \
    "printf '\\033[2J" \
    "\\033[1;1HAAAAA" \
    "\\033[2;1HBBBBB\\033[2;3H\\033[0K" \
    "\\033[3;1HCCCCC\\033[3;1H\\033[2K'"

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
        { 0, "AAAAA" },
        { 1, "BB" },
        { 2, "" },
    };

    TEST_START;

    TEST_STEP("Open a session; skip when no backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_erase"));
    if (!tapi_shell_available(sess.pco))
        TEST_SKIP("The agent has no terminal-emulator backend");

    TEST_STEP("Paint a screen and erase parts of it");
    CHECK_RC(tsapi_shell_open_sh(&sess, ERASE_SCRIPT, 80, 24, &sh));
    opened = true;
    CHECK_RC(tapi_shell_wait(&sh, 5000, &status));
    if (!status.exited || status.exit_code != 0)
        TEST_VERDICT("the painter did not exit cleanly (exited=%d code=%d)",
                     status.exited, status.exit_code);

    TEST_STEP("Only the cells the escapes named were cleared");
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

    TEST_STEP("Chart the surviving character count per row");
    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    for (y = 0; y < (int)TE_ARRAY_LEN(expect); y++)
    {
        te_string row = TE_STRING_INIT;
        int       n;

        CHECK_RC(tapi_shell_row_text(&sh, expect[y].row, &row));
        n = row.ptr != NULL ? (int)strlen(row.ptr) : 0;
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "row", TE_MI_MEAS_AGGR_SINGLE, y,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "surviving", TE_MI_MEAS_AGGR_SINGLE, n,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_string_free(&row);
    }
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "erase", "Surviving characters per row");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "erase",
                               TE_MI_GRAPH_AXIS_X, "row");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "erase", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "surviving");
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
