/** @file
 * @brief Shell Group
 *
 * Horizontal tabs land on tab stops: the default stops every eight
 * columns, and a custom stop set with HTS (after clearing all stops
 * with TBC) is honoured too. A row of tab-separated letters is printed
 * and the column each letter lands in is read from its cell.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "shell/tabs"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/*
 * Row 0, default stops (every 8): A at col 0, B at col 8, C at col 16.
 * Then clear every stop (CSI 3 g), set one stop at col 5 (HTS at 2;6H).
 * Row 1: X at col 0, tab -> the only stop, col 5, Y.
 */
#define TABS_SCRIPT \
    "printf '\\033[2J\\033[1;1HA\\tB\\tC" \
    "\\033[3g\\033[2;6H\\033H\\033[2;1HX\\tY'"

int
main(int argc, char **argv)
{
    tsapi_shell_sess   sess;
    tapi_shell_session sh;
    tapi_shell_status  status;
    te_mi_logger      *logger = NULL;
    bool               opened = false;
    unsigned int       i;
    static const struct {
        int      x, y;
        uint32_t cp;
    } expect[] = {
        {  0, 0, 'A' },
        {  8, 0, 'B' },
        { 16, 0, 'C' },
        {  0, 1, 'X' },
        {  5, 1, 'Y' },
    };

    TEST_START;

    TEST_STEP("Open a session; skip when no backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_tabs"));
    if (!tapi_shell_available(sess.pco))
        TEST_SKIP("The agent has no terminal-emulator backend");

    TEST_STEP("Print tab-separated letters with default and custom stops");
    CHECK_RC(tsapi_shell_open_sh(&sess, TABS_SCRIPT, 80, 24, &sh));
    opened = true;
    CHECK_RC(tapi_shell_wait(&sh, 5000, &status));
    if (!status.exited || status.exit_code != 0)
        TEST_VERDICT("the script did not exit cleanly (exited=%d code=%d)",
                     status.exited, status.exit_code);

    TEST_STEP("Each letter landed on its expected tab stop");
    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    for (i = 0; i < TE_ARRAY_LEN(expect); i++)
    {
        tapi_shell_cell_t c;

        CHECK_RC(tapi_shell_cell(&sh, expect[i].x, expect[i].y, &c));
        RING("cell(%d,%d) = U+%04X (want U+%04X)", expect[i].x, expect[i].y,
             c.codepoint, expect[i].cp);
        if (c.codepoint != expect[i].cp)
            TEST_VERDICT("cell(%d,%d) is U+%04X, expected U+%04X ('%c')",
                         expect[i].x, expect[i].y, c.codepoint, expect[i].cp,
                         (char)expect[i].cp);

        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "letter", TE_MI_MEAS_AGGR_SINGLE, i,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "column", TE_MI_MEAS_AGGR_SINGLE, expect[i].x,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
    }
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "tabs", "Column reached by each tab");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "tabs",
                               TE_MI_GRAPH_AXIS_X, "letter");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "tabs", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "column");
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
