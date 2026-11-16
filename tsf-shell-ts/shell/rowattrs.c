/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Shell Group
 *
 * A whole row's per-cell attributes are dumped in one call as
 * tab-separated records (column, code point, colours, palette indices,
 * flags). A row with a coloured cell, a bold cell and a plain cell is
 * painted; the records are parsed and each cell's code point, colour
 * index and flags are asserted.
 */

#define TE_TEST_NAME    "shell/rowattrs"

#include "te_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/* Cell 0 'R' red fg (idx 1); cell 1 'b' bold; cell 2 'p' plain. */
#define RA_SCRIPT \
    "printf '\\033[2J\\033[1;1H\\033[31mR\\033[0m\\033[1mb\\033[0mp'"

#define MAXW 16

int
main(int argc, char **argv)
{
    tsapi_shell_sess   sess;
    const char        *backend;
    tapi_shell_session sh;
    tapi_shell_status  status;
    te_mi_logger      *logger = NULL;
    te_string          rec = TE_STRING_INIT;
    bool               opened = false;
    char              *saveptr = NULL;
    char              *line;
    int                parsed = 0;
    struct {
        bool         seen;
        unsigned int cp;
        int          fg_index;
        unsigned int flags;
    } cell[MAXW];

    TEST_START;
    TEST_GET_STRING_PARAM(backend);

    memset(cell, 0, sizeof(cell));

    TEST_STEP("Open a session; skip when the agent lacks the backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_rowattrs"));
    if (!tsapi_shell_sess_select(&sess, backend))
        TEST_SKIP("The agent does not have this terminal-emulator backend");

    TEST_STEP("Paint a row with a coloured, a bold and a plain cell");
    CHECK_RC(tsapi_shell_open_sh(&sess, RA_SCRIPT, 80, 24, &sh));
    opened = true;
    CHECK_RC(tapi_shell_wait(&sh, 5000, &status));
    if (!status.exited || status.exit_code != 0)
        TEST_VERDICT("the painter did not exit cleanly (exited=%d code=%d)",
                     status.exited, status.exit_code);

    TEST_STEP("Dump the row's attribute records and parse them");
    CHECK_RC(tapi_shell_row_attrs(&sh, 0, &rec));
    if (rec.ptr == NULL)
        TEST_VERDICT("row 0 produced no attribute records");

    for (line = strtok_r(rec.ptr, "\n", &saveptr); line != NULL;
         line = strtok_r(NULL, "\n", &saveptr))
    {
        int          x = -1, fg = 0, bg = 0, fgi = 0, bgi = 0;
        unsigned int cp = 0, fl = 0;

        if (sscanf(line, "%d\t%u\t%d\t%d\t%d\t%d\t%u",
                   &x, &cp, &fg, &bg, &fgi, &bgi, &fl) != 7)
            continue;
        parsed++;
        if (x >= 0 && x < MAXW)
        {
            cell[x].seen = true;
            cell[x].cp = cp;
            cell[x].fg_index = fgi;
            cell[x].flags = fl;
        }
    }
    RING("parsed %d attribute records", parsed);
    if (parsed == 0)
        TEST_VERDICT("no attribute record parsed from '%s'", rec.ptr);

    TEST_STEP("The coloured cell carries its code point and palette index");
    if (!cell[0].seen || cell[0].cp != (uint32_t)'R' || cell[0].fg_index != 1)
        TEST_VERDICT("cell 0: seen=%d cp=U+%04X fg_index=%d, expected 'R' idx1",
                     (int)cell[0].seen, cell[0].cp, cell[0].fg_index);

    TEST_STEP("The bold cell carries the bold flag");
    if (!cell[1].seen || cell[1].cp != (uint32_t)'b' ||
        (cell[1].flags & TAPI_SHELL_ATTR_BOLD) == 0)
        TEST_VERDICT("cell 1: seen=%d cp=U+%04X flags=0x%x, expected 'b' bold",
                     (int)cell[1].seen, cell[1].cp, cell[1].flags);

    TEST_STEP("The plain cell carries no colour index and no flags");
    if (!cell[2].seen || cell[2].cp != (uint32_t)'p' ||
        cell[2].fg_index != -1 || cell[2].flags != 0)
        TEST_VERDICT("cell 2: seen=%d cp=U+%04X fg_index=%d flags=0x%x, "
                     "expected 'p' plain", (int)cell[2].seen, cell[2].cp,
                     cell[2].fg_index, cell[2].flags);

    TEST_STEP("Chart the flags bitmask across the first cells");
    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    {
        int x;

        for (x = 0; x < 3; x++)
        {
            te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                                  "cell", TE_MI_MEAS_AGGR_SINGLE, x,
                                  TE_MI_MEAS_MULTIPLIER_PLAIN);
            te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                                  "flags", TE_MI_MEAS_AGGR_SINGLE,
                                  (int)cell[x].flags,
                                  TE_MI_MEAS_MULTIPLIER_PLAIN);
        }
    }
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "ra", "Attribute flags across the row");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "ra",
                               TE_MI_GRAPH_AXIS_X, "cell");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "ra", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "flags");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    te_string_free(&rec);
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    if (opened)
        tapi_shell_close(&sh);
    tsapi_shell_sess_fini(&sess);
    TEST_END;
}
