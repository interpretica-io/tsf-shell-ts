/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Shell Group
 *
 * The emulator as the thing under test: a fixed escape script is fed in
 * and the whole rendered grid is compared to a golden expectation - a
 * programmatic vttest. Each expected cell names its code point and,
 * where it matters, its foreground palette index and a required
 * attribute flag; every cell must match.
 */

#define TE_TEST_NAME    "shell/conformance"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/*
 * Row 0: "ABC" plain.
 * Row 1: red R, green G, yellow Y.
 * Row 2: bold X, underline U.
 */
#define CONF_SCRIPT \
    "printf '\\033[2J" \
    "\\033[1;1HABC" \
    "\\033[2;1H\\033[31mR\\033[32mG\\033[33mY\\033[0m" \
    "\\033[3;1H\\033[1mX\\033[0m\\033[4mU\\033[0m'"

#define DONT_CARE   (-2)   /* fg_index value meaning "do not check colour" */

int
main(int argc, char **argv)
{
    tsapi_shell_sess   sess;
    const char        *backend;
    tapi_shell_session sh;
    tapi_shell_status  status;
    te_mi_logger      *logger = NULL;
    bool               opened = false;
    unsigned int       i;
    int                matched = 0;
    static const struct {
        int          x, y;
        uint32_t     cp;
        int          fg_index;   /* DONT_CARE to skip */
        unsigned int req_flags;  /* flags that must all be set */
    } golden[] = {
        { 0, 0, 'A', DONT_CARE, 0 },
        { 1, 0, 'B', DONT_CARE, 0 },
        { 2, 0, 'C', DONT_CARE, 0 },
        { 0, 1, 'R', 1, 0 },
        { 1, 1, 'G', 2, 0 },
        { 2, 1, 'Y', 3, 0 },
        { 0, 2, 'X', DONT_CARE, TAPI_SHELL_ATTR_BOLD },
        { 1, 2, 'U', DONT_CARE, TAPI_SHELL_ATTR_UNDERLINE },
    };

    TEST_START;
    TEST_GET_STRING_PARAM(backend);

    TEST_STEP("Open a session; skip when the agent lacks the backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_conf"));
    if (!tsapi_shell_sess_select(&sess, backend))
        TEST_SKIP("The agent does not have this terminal-emulator backend");

    TEST_STEP("Feed the fixed escape script and let it render");
    CHECK_RC(tsapi_shell_open_sh(&sess, CONF_SCRIPT, 80, 24, &sh));
    opened = true;
    CHECK_RC(tapi_shell_wait(&sh, 5000, &status));
    if (!status.exited || status.exit_code != 0)
        TEST_VERDICT("the script did not exit cleanly (exited=%d code=%d)",
                     status.exited, status.exit_code);

    TEST_STEP("Every golden cell matches the rendered grid");
    for (i = 0; i < TE_ARRAY_LEN(golden); i++)
    {
        tapi_shell_cell_t c;
        bool              ok = true;

        CHECK_RC(tapi_shell_cell(&sh, golden[i].x, golden[i].y, &c));

        if (c.codepoint != golden[i].cp)
            ok = false;
        if (golden[i].fg_index != DONT_CARE && c.fg_index != golden[i].fg_index)
            ok = false;
        if ((c.flags & golden[i].req_flags) != golden[i].req_flags)
            ok = false;

        if (!ok)
            ERROR("cell(%d,%d): cp=U+%04X fg_index=%d flags=0x%x; "
                  "expected cp=U+%04X fg_index=%d flags&0x%x",
                  golden[i].x, golden[i].y, c.codepoint, c.fg_index, c.flags,
                  golden[i].cp, golden[i].fg_index, golden[i].req_flags);
        else
            matched++;
    }

    TEST_STEP("Chart cells matched against the golden total");
    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "matched", TE_MI_MEAS_AGGR_SINGLE, matched,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "total", TE_MI_MEAS_AGGR_SINGLE,
                          (int)TE_ARRAY_LEN(golden),
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "conf", "Golden cells matched");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "conf",
                               TE_MI_GRAPH_AXIS_X, "matched");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "conf", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "total");
    te_mi_logger_destroy(logger);
    logger = NULL;

    if (matched != (int)TE_ARRAY_LEN(golden))
        TEST_VERDICT("%d of %d golden cells matched the rendered grid",
                     matched, (int)TE_ARRAY_LEN(golden));

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    if (opened)
        tapi_shell_close(&sh);
    tsapi_shell_sess_fini(&sess);
    TEST_END;
}
