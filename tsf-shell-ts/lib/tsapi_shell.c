/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Suite helpers
 *
 * @author Maxim Menshikov <maxim.menshikov@interpretica.io>
 */

#define TE_LGR_USER "TSAPI SHELL"

#include "te_config.h"

#include <string.h>

#include "logger_api.h"
#include "te_errno.h"

#include "tsapi_shell.h"

/* See description in tsapi_shell.h */
te_errno
tsapi_shell_sess_init(tsapi_shell_sess *sess, const char *name)
{
    te_errno rc;

    memset(sess, 0, sizeof(*sess));
    sess->ta = TSAPI_SHELL_TA;

    rc = rcf_rpc_server_create(sess->ta, name, &sess->pco);
    if (rc != 0)
    {
        ERROR("Cannot create the RPC server '%s' on %s: %r", name, sess->ta,
              rc);
        return rc;
    }

    return 0;
}

/* See description in tsapi_shell.h */
te_errno
tsapi_shell_open_sh(tsapi_shell_sess *sess, const char *script, int cols,
                    int rows, tapi_shell_session *sh)
{
    const char *argv[] = { "sh", "-c", script, NULL };

    return tapi_shell_open(sess->pco, argv, cols, rows, NULL, sh);
}

/* See description in tsapi_shell.h */
void
tsapi_shell_sess_fini(tsapi_shell_sess *sess)
{
    if (sess->pco != NULL)
    {
        rcf_rpc_server_destroy(sess->pco);
        sess->pco = NULL;
    }
}
