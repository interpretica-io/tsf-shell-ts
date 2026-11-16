/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Shell Group
 *
 * SGR colour and attribute sequences produce cells whose palette index,
 * foreground/background and bold/underline/reverse flags are what was
 * set. ANSI colours 30-37/40-47 map to palette indices 0-7, which is
 * deterministic across the default palettes, so the test asserts on the
 * index rather than an RGB value.
 */

#define TE_TEST_NAME    "shell/attrs"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/*
 * Row 0, cell by cell:
 *   0 'R' fg red(1)
 *   1 'G' fg green(2)
 *   2 'B' fg green(2) bg blue(4)
 *   3 'b' bold           (after reset)
 *   4 'u' bold+underline
 *   5 'r' bold+underline+reverse
 */
#define ATTRS_SCRIPT \
    "printf '\\033[2J\\033[1;1H" \
    "\\033[31mR\\033[32mG\\033[44mB\\033[0m" \
    "\\033[1mb\\033[4mu\\033[7mr\\033[0m'"

int
main(int argc, char **argv)
{
    tsapi_shell_sess   sess;
    const char        *backend;
    tapi_shell_session sh;
    tapi_shell_status  status;
    te_mi_logger      *logger = NULL;
    bool               opened = false;
    tapi_shell_cell_t  c;
    int                x;

    TEST_START;
    TEST_GET_STRING_PARAM(backend);

    TEST_STEP("Open a session; skip when the agent lacks the backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_attrs"));
    if (!tsapi_shell_sess_select(&sess, backend))
        TEST_SKIP("The agent does not have this terminal-emulator backend");

    TEST_STEP("Spawn a shell that prints coloured/attributed cells");
    CHECK_RC(tsapi_shell_open_sh(&sess, ATTRS_SCRIPT, 80, 24, &sh));
    opened = true;
    CHECK_RC(tapi_shell_wait(&sh, 5000, &status));

    TEST_STEP("The red cell has foreground palette index 1");
    CHECK_RC(tapi_shell_cell(&sh, 0, 0, &c));
    if (c.codepoint != (uint32_t)'R' || c.fg_index != 1)
        TEST_VERDICT("cell(0,0) cp=U+%04X fg_index=%d, expected 'R' idx 1",
                     c.codepoint, c.fg_index);

    TEST_STEP("The green cell has foreground palette index 2");
    CHECK_RC(tapi_shell_cell(&sh, 1, 0, &c));
    if (c.fg_index != 2)
        TEST_VERDICT("cell(1,0) fg_index=%d, expected 2", c.fg_index);

    TEST_STEP("The green-on-blue cell has fg 2 and bg 4");
    CHECK_RC(tapi_shell_cell(&sh, 2, 0, &c));
    if (c.fg_index != 2 || c.bg_index != 4)
        TEST_VERDICT("cell(2,0) fg_index=%d bg_index=%d, expected 2/4",
                     c.fg_index, c.bg_index);

    TEST_STEP("Bold, underline and reverse flags are set as printed");
    CHECK_RC(tapi_shell_cell(&sh, 3, 0, &c));
    if ((c.flags & TAPI_SHELL_ATTR_BOLD) == 0)
        TEST_VERDICT("cell(3,0) is not bold (flags 0x%x)", c.flags);
    CHECK_RC(tapi_shell_cell(&sh, 4, 0, &c));
    if ((c.flags & (TAPI_SHELL_ATTR_BOLD | TAPI_SHELL_ATTR_UNDERLINE)) !=
        (TAPI_SHELL_ATTR_BOLD | TAPI_SHELL_ATTR_UNDERLINE))
        TEST_VERDICT("cell(4,0) is not bold+underline (flags 0x%x)", c.flags);
    CHECK_RC(tapi_shell_cell(&sh, 5, 0, &c));
    if ((c.flags & TAPI_SHELL_ATTR_REVERSE) == 0)
        TEST_VERDICT("cell(5,0) is not reverse (flags 0x%x)", c.flags);

    TEST_STEP("Chart the colour index of each cell");
    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    for (x = 0; x < 6; x++)
    {
        CHECK_RC(tapi_shell_cell(&sh, x, 0, &c));
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "cell", TE_MI_MEAS_AGGR_SINGLE, x,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "fg_index", TE_MI_MEAS_AGGR_SINGLE, c.fg_index,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "bg_index", TE_MI_MEAS_AGGR_SINGLE, c.bg_index,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
    }
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "sgr", "Cell colour index across the row");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "sgr",
                               TE_MI_GRAPH_AXIS_X, "cell");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "sgr", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "fg_index");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "sgr", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "bg_index");
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
