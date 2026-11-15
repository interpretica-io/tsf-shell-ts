/** @file
 * @brief Shell Group
 *
 * The alternate screen buffer (DECSET 1049, used by full-screen TUIs)
 * swaps the screen out and back: entering it hides the primary content,
 * and leaving it restores the primary content exactly. A raw passthrough
 * (@c cat) carries the escapes; the screen is snapshotted on the primary
 * buffer, on the alternate buffer, and after returning to the primary.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "shell/altscreen"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/*
 * Raw passthrough: whatever is typed is emitted verbatim and rendered.
 * It announces readiness (RDY) only once the tty is in raw mode, so our
 * escapes are never processed by a cooked line discipline (which would
 * echo and mangle them). The first payload's erase wipes the RDY mark.
 */
#define ALT_SCRIPT "stty raw -echo; printf RDY; exec cat"

/* Wait for @p token to appear on the rendered screen. */
static te_errno
wait_screen(tapi_shell_session *sh, const char *token)
{
    tapi_shell_expect_opts opts = {
        .where = TAPI_SHELL_WHERE_SCREEN,
        .timeout_ms = 3000,
    };

    return tapi_shell_expect(sh, token, &opts, NULL);
}

int
main(int argc, char **argv)
{
    tsapi_shell_sess   sess;
    tapi_shell_session sh;
    te_mi_logger      *logger = NULL;
    bool               opened = false;

    TEST_START;

    TEST_STEP("Open a session; skip when no backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_altscreen"));
    if (!tapi_shell_available(sess.pco))
        TEST_SKIP("The agent has no terminal-emulator backend");

    TEST_STEP("Spawn a raw passthrough that renders what it receives");
    CHECK_RC(tsapi_shell_open_sh(&sess, ALT_SCRIPT, 80, 24, &sh));
    opened = true;

    TEST_STEP("Wait until the passthrough is in raw mode");
    rc = tapi_shell_expect_str(&sh, "RDY", 5000);
    if (rc != 0)
        TEST_VERDICT("the passthrough never reached raw mode (%r)", rc);

    TEST_STEP("Paint the primary screen");
    CHECK_RC(tapi_shell_send(&sh, "\033[2J\033[1;1HPRIMARY"));
    rc = wait_screen(&sh, "PRIMARY");
    if (rc != 0)
        TEST_VERDICT("the primary content never rendered (%r)", rc);

    TEST_STEP("Enter the alternate screen; the primary content is hidden");
    CHECK_RC(tapi_shell_send(&sh, "\033[?1049h\033[2J\033[1;1HALT"));
    rc = wait_screen(&sh, "ALT");
    if (rc != 0)
        TEST_VERDICT("the alternate content never rendered (%r)", rc);
    if (tapi_shell_screen_contains(&sh, "PRIMARY"))
        TEST_VERDICT("the primary content is still visible on the alt screen");

    TEST_STEP("Leave the alternate screen; the primary content is restored");
    CHECK_RC(tapi_shell_send(&sh, "\033[?1049l"));
    rc = wait_screen(&sh, "PRIMARY");
    if (rc != 0)
        TEST_VERDICT("the primary content was not restored (%r)", rc);
    if (tapi_shell_screen_contains(&sh, "ALT"))
        TEST_VERDICT("the alternate content leaked back onto the primary");

    TEST_STEP("Chart the buffer occupancy across the three phases");
    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    {
        int phase;
        /* 1 = primary shown, 2 = alt shown, 1 = primary restored. */
        int shown[3] = { 1, 2, 1 };

        for (phase = 0; phase < 3; phase++)
        {
            te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                                  "phase", TE_MI_MEAS_AGGR_SINGLE, phase,
                                  TE_MI_MEAS_MULTIPLIER_PLAIN);
            te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                                  "buffer", TE_MI_MEAS_AGGR_SINGLE,
                                  shown[phase], TE_MI_MEAS_MULTIPLIER_PLAIN);
        }
    }
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "alt", "Active buffer across the phases");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "alt",
                               TE_MI_GRAPH_AXIS_X, "phase");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "alt", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "buffer");
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
