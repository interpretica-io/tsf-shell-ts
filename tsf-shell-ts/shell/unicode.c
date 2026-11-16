/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Shell Group
 *
 * A UTF-8 stream is decoded to code points, and an East-Asian wide
 * glyph occupies two columns and is flagged as wide. A CJK ideograph
 * followed by an accented Latin letter is printed; the wide cell's code
 * point and width flag, and the following cell's code point, are read
 * back.
 */

#define TE_TEST_NAME    "shell/unicode"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/*
 * U+4E16 (CJK, wide, bytes E4 B8 96) then U+00E9 'e-acute' (bytes C3 A9).
 * The wide glyph sits in cells 0-1; the accented letter in cell 2.
 */
#define UNI_SCRIPT \
    "printf '\\033[2J\\033[1;1H\\344\\270\\226\\303\\251'"

#define CP_WIDE   0x4E16u
#define CP_ACCENT 0x00E9u

int
main(int argc, char **argv)
{
    tsapi_shell_sess   sess;
    const char        *backend;
    tapi_shell_session sh;
    tapi_shell_status  status;
    te_mi_logger      *logger = NULL;
    bool               opened = false;
    tapi_shell_cell_t  wide;
    tapi_shell_cell_t  accent;

    TEST_START;
    TEST_GET_STRING_PARAM(backend);

    TEST_STEP("Open a session; skip when the agent lacks the backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_unicode"));
    if (!tsapi_shell_sess_select(&sess, backend))
        TEST_SKIP("The agent does not have this terminal-emulator backend");

    TEST_STEP("Print a wide CJK glyph and an accented Latin letter");
    CHECK_RC(tsapi_shell_open_sh(&sess, UNI_SCRIPT, 80, 24, &sh));
    opened = true;
    CHECK_RC(tapi_shell_wait(&sh, 5000, &status));
    if (!status.exited || status.exit_code != 0)
        TEST_VERDICT("the script did not exit cleanly (exited=%d code=%d)",
                     status.exited, status.exit_code);

    TEST_STEP("The wide glyph decoded to its code point and is flagged wide");
    CHECK_RC(tapi_shell_cell(&sh, 0, 0, &wide));
    RING("cell(0,0) cp=U+%04X flags=0x%x", wide.codepoint, wide.flags);
    if (wide.codepoint != CP_WIDE)
        TEST_VERDICT("cell(0,0) is U+%04X, expected U+%04X", wide.codepoint,
                     CP_WIDE);
    if ((wide.flags & TAPI_SHELL_ATTR_WIDE) == 0)
        TEST_VERDICT("the wide glyph is not flagged wide (flags 0x%x)",
                     wide.flags);

    TEST_STEP("The accented letter sits in the cell past the wide glyph");
    CHECK_RC(tapi_shell_cell(&sh, 2, 0, &accent));
    RING("cell(2,0) cp=U+%04X", accent.codepoint);
    if (accent.codepoint != CP_ACCENT)
        TEST_VERDICT("cell(2,0) is U+%04X, expected U+%04X", accent.codepoint,
                     CP_ACCENT);

    TEST_STEP("Chart the decoded code point of each read cell");
    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "cell", TE_MI_MEAS_AGGR_SINGLE, 0,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "codepoint", TE_MI_MEAS_AGGR_SINGLE,
                          (int)wide.codepoint, TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "cell", TE_MI_MEAS_AGGR_SINGLE, 2,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "codepoint", TE_MI_MEAS_AGGR_SINGLE,
                          (int)accent.codepoint, TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "uni", "Decoded code point per cell");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "uni",
                               TE_MI_GRAPH_AXIS_X, "cell");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "uni", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "codepoint");
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
