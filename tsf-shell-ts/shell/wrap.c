/** @file
 * @brief Shell Group
 *
 * Autowrap (DECAWM, on by default) carries text past the last column
 * onto the next row rather than piling it in the corner. On a narrow
 * screen a line longer than the width is printed and the two rows it
 * wraps onto are read back.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "shell/wrap"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/* Ten characters on a five-column screen wrap as "ABCDE" / "FGHIJ". */
#define WRAP_SCRIPT "printf '\\033[2J\\033[1;1HABCDEFGHIJ'"
#define WRAP_COLS   5

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
        { 0, "ABCDE" },
        { 1, "FGHIJ" },
    };

    TEST_START;

    TEST_STEP("Open a session; skip when no backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_wrap"));
    if (!tapi_shell_available(sess.pco))
        TEST_SKIP("The agent has no terminal-emulator backend");

    TEST_STEP("Print a line longer than the five-column screen");
    CHECK_RC(tsapi_shell_open_sh(&sess, WRAP_SCRIPT, WRAP_COLS, 24, &sh));
    opened = true;
    CHECK_RC(tapi_shell_wait(&sh, 5000, &status));
    if (!status.exited || status.exit_code != 0)
        TEST_VERDICT("the script did not exit cleanly (exited=%d code=%d)",
                     status.exited, status.exit_code);

    TEST_STEP("The line wrapped onto the next row");
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

    TEST_STEP("Chart the characters landing on each wrapped row");
    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    for (y = 0; y < (int)TE_ARRAY_LEN(expect); y++)
    {
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "row", TE_MI_MEAS_AGGR_SINGLE, y,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "chars", TE_MI_MEAS_AGGR_SINGLE,
                              (int)strlen(expect[y].text),
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
    }
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "wrap", "Characters per wrapped row");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "wrap",
                               TE_MI_GRAPH_AXIS_X, "row");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "wrap", TE_MI_GRAPH_AXIS_Y,
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
