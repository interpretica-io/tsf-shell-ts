/** @file
 * @brief Shell Group
 *
 * The emulator carries full colour: a 24-bit direct-colour SGR sets an
 * exact 0xRRGGBB foreground and background, and a 256-colour SGR sets a
 * palette index. Three cells are printed, each exercising one of these,
 * and their reported colours are asserted exactly.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "shell/color"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/*
 * Cell 0 'T': fg 24-bit (17,34,51)   = 0x112233.
 * Cell 1 'B': + bg 24-bit (68,85,102) = 0x445566 (fg still 0x112233).
 * Cell 2 'I': reset, then fg 256-colour palette index 39.
 */
#define COLOR_SCRIPT \
    "printf '\\033[2J\\033[1;1H" \
    "\\033[38;2;17;34;51mT" \
    "\\033[48;2;68;85;102mB" \
    "\\033[0m\\033[38;5;39mI\\033[0m'"

#define FG_RGB 0x112233
#define BG_RGB 0x445566
#define IDX256 39

int
main(int argc, char **argv)
{
    tsapi_shell_sess   sess;
    tapi_shell_session sh;
    tapi_shell_status  status;
    te_mi_logger      *logger = NULL;
    bool               opened = false;
    tapi_shell_cell_t  t, b, ix;

    TEST_START;

    TEST_STEP("Open a session; skip when no backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_color"));
    if (!tapi_shell_available(sess.pco))
        TEST_SKIP("The agent has no terminal-emulator backend");

    TEST_STEP("Print direct-colour and 256-colour cells");
    CHECK_RC(tsapi_shell_open_sh(&sess, COLOR_SCRIPT, 80, 24, &sh));
    opened = true;
    CHECK_RC(tapi_shell_wait(&sh, 5000, &status));
    if (!status.exited || status.exit_code != 0)
        TEST_VERDICT("the script did not exit cleanly (exited=%d code=%d)",
                     status.exited, status.exit_code);

    TEST_STEP("The 24-bit foreground is reported as an exact RGB");
    CHECK_RC(tapi_shell_cell(&sh, 0, 0, &t));
    RING("cell(0,0) fg=0x%06x", t.fg < 0 ? 0 : t.fg);
    if (t.fg != FG_RGB)
        TEST_VERDICT("cell(0,0) fg is 0x%06x, expected 0x%06x", t.fg, FG_RGB);

    TEST_STEP("The 24-bit background is reported as an exact RGB");
    CHECK_RC(tapi_shell_cell(&sh, 1, 0, &b));
    RING("cell(1,0) fg=0x%06x bg=0x%06x", b.fg < 0 ? 0 : b.fg,
         b.bg < 0 ? 0 : b.bg);
    if (b.bg != BG_RGB)
        TEST_VERDICT("cell(1,0) bg is 0x%06x, expected 0x%06x", b.bg, BG_RGB);
    if (b.fg != FG_RGB)
        TEST_VERDICT("cell(1,0) fg is 0x%06x, expected 0x%06x", b.fg, FG_RGB);

    TEST_STEP("The 256-colour foreground is reported as a palette index");
    CHECK_RC(tapi_shell_cell(&sh, 2, 0, &ix));
    RING("cell(2,0) fg_index=%d", ix.fg_index);
    if (ix.fg_index != IDX256)
        TEST_VERDICT("cell(2,0) fg_index is %d, expected %d", ix.fg_index,
                     IDX256);

    TEST_STEP("Chart each cell's reported colour channel sum");
    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    {
        int sums[3];
        int k;

        sums[0] = ((FG_RGB >> 16) & 0xff) + ((FG_RGB >> 8) & 0xff) +
                  (FG_RGB & 0xff);
        sums[1] = ((BG_RGB >> 16) & 0xff) + ((BG_RGB >> 8) & 0xff) +
                  (BG_RGB & 0xff);
        sums[2] = IDX256;
        for (k = 0; k < 3; k++)
        {
            te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                                  "cell", TE_MI_MEAS_AGGR_SINGLE, k,
                                  TE_MI_MEAS_MULTIPLIER_PLAIN);
            te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                                  "value", TE_MI_MEAS_AGGR_SINGLE, sums[k],
                                  TE_MI_MEAS_MULTIPLIER_PLAIN);
        }
    }
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "color", "Reported colour value per cell");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "color",
                               TE_MI_GRAPH_AXIS_X, "cell");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "color", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "value");
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
