/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Shell Group
 *
 * In-line editing control sequences rearrange a row without repainting
 * it: delete-character (DCH) pulls the tail left, insert-character (ICH)
 * pushes it right. A known row has each applied and the resulting text
 * is read back exactly, interior blanks and all.
 */

#define TE_TEST_NAME    "shell/edit"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/*
 * Row 0: "ABCDEF"; at column 3 delete 2 chars (C,D) -> "ABEF".
 * Row 1: "abcdef"; at column 3 insert 2 blanks     -> "ab  cdef".
 */
#define EDIT_SCRIPT \
    "printf '\\033[2J" \
    "\\033[1;1HABCDEF\\033[1;3H\\033[2P" \
    "\\033[2;1Habcdef\\033[2;3H\\033[2@'"

int
main(int argc, char **argv)
{
    tsapi_shell_sess   sess;
    const char        *backend;
    tapi_shell_session sh;
    tapi_shell_status  status;
    te_mi_logger      *logger = NULL;
    bool               opened = false;
    int                y;
    static const struct {
        int         row;
        const char *text;
    } expect[] = {
        { 0, "ABEF" },
        { 1, "ab  cdef" },
    };

    TEST_START;
    TEST_GET_STRING_PARAM(backend);

    TEST_STEP("Open a session; skip when the agent lacks the backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_edit"));
    if (!tsapi_shell_sess_select(&sess, backend))
        TEST_SKIP("The agent does not have this terminal-emulator backend");

    TEST_STEP("Apply delete-character and insert-character to known rows");
    CHECK_RC(tsapi_shell_open_sh(&sess, EDIT_SCRIPT, 80, 24, &sh));
    opened = true;
    CHECK_RC(tapi_shell_wait(&sh, 5000, &status));
    if (!status.exited || status.exit_code != 0)
        TEST_VERDICT("the script did not exit cleanly (exited=%d code=%d)",
                     status.exited, status.exit_code);

    TEST_STEP("Each edited row reads back exactly");
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

    TEST_STEP("Chart the resulting length of each edited row");
    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    for (y = 0; y < (int)TE_ARRAY_LEN(expect); y++)
    {
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "row", TE_MI_MEAS_AGGR_SINGLE, y,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "len", TE_MI_MEAS_AGGR_SINGLE,
                              (int)strlen(expect[y].text),
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
    }
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "edit", "Row length after in-line editing");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "edit",
                               TE_MI_GRAPH_AXIS_X, "row");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "edit", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "len");
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
