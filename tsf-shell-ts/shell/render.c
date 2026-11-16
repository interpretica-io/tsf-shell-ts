/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Shell Group
 *
 * A shell prints a known screen with cursor-positioning escape
 * sequences; tsf-shell renders it and the text is read back and
 * asserted row by row. The oracle is the script: the text at each row
 * is exactly what printf placed there.
 */

#define TE_TEST_NAME    "shell/render"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/* Paint three known lines at known rows, then exit. */
#define RENDER_SCRIPT \
    "printf '\\033[2J\\033[1;1HLOGIN:\\033[2;1Husername\\033[3;1Hpassword'"

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
        { 0, "LOGIN:" },
        { 1, "username" },
        { 2, "password" },
    };

    TEST_START;
    TEST_GET_STRING_PARAM(backend);

    TEST_STEP("Open a session; skip when the agent lacks the backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_render"));
    if (!tsapi_shell_sess_select(&sess, backend))
        TEST_SKIP("The agent does not have this terminal-emulator backend");

    TEST_STEP("Spawn a shell that paints the screen and exits");
    CHECK_RC(tsapi_shell_open_sh(&sess, RENDER_SCRIPT, 80, 24, &sh));
    opened = true;
    CHECK_RC(tapi_shell_wait(&sh, 5000, &status));
    if (!status.exited || status.exit_code != 0)
        TEST_VERDICT("the painter did not exit cleanly (exited=%d code=%d)",
                     status.exited, status.exit_code);

    TEST_STEP("Each painted row reads back exactly");
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

    TEST_STEP("Chart the characters painted per row");
    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    {
        int                 rows = 0;
        tapi_shell_screen_t screen;

        CHECK_RC(tapi_shell_screen(&sh, &screen));
        rows = screen.rows;
        tapi_shell_screen_free(&screen);

        for (y = 0; y < rows; y++)
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
    }
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "paint", "Characters painted per row");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "paint",
                               TE_MI_GRAPH_AXIS_X, "row");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "paint", TE_MI_GRAPH_AXIS_Y,
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
