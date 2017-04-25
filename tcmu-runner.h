/*
 * Copyright 2014, Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
*/

/*
 * This header defines the interface between tcmu-runner and its loadable
 * subtype handlers.
 */

#ifndef __TCMU_RUNNER_H
#define __TCMU_RUNNER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <sys/uio.h>
#include "scsi_defs.h"
#include "libtcmu_log.h"
#include "libtcmu_common.h"
#include "alua.h"

typedef int (*rw_fn_t)(struct tcmu_device *, struct tcmulib_cmd *,
		       struct iovec *, size_t, size_t, off_t);
typedef int (*flush_fn_t)(struct tcmu_device *, struct tcmulib_cmd *);
typedef int (*handle_cmd_fn_t)(struct tcmu_device *, struct tcmulib_cmd *);

struct tcmur_handler {
	const char *name;	/* Human-friendly name */
	const char *subtype;	/* Name for cfgstring matching */
	const char *cfg_desc;	/* Description of this backstore's config string */

	void *opaque;		/* Handler private data. */

	/*
	 * As much as possible, check that the cfgstring will result
	 * in a working device when given to us as dev->cfgstring in
	 * the ->open() call.
	 *
	 * This function is optional but gives configuration tools a
	 * chance to warn users in advance if the device they're
	 * trying to create is invalid.
	 *
	 * Returns true if string is valid. Only if false, set *reason
	 * to a string that says why. The string will be free()ed.
	 * Suggest using asprintf().
	 */
	bool (*check_config)(const char *cfgstring, char **reason);

	/* Per-device added/removed callbacks */
	int (*open)(struct tcmu_device *dev);
	void (*close)(struct tcmu_device *dev);

	/*
	 * If > 0, runner will execute up to nr_threads IO callouts from
	 * threads.
	 * if 0, runner will call IO callouts from the cmd proc thread or
	 * completion context for compound commands.
	 */
	int nr_threads;

	/*
	 * Synchronously change the state to new_state.
	 * Returns
	 * - SAM_STAT_GOOD on success.
	 * - SCSI status with sense set if needed on failure.
	 */
	tcmu_transition_state_fn_t *transition_state;
	tcmu_report_state_fn_t *report_state;

	/*
	 * Async handle_cmd only handlers return:
	 *
	 * - SCSI status if handled (either good/bad)
	 * - TCMU_NOT_HANDLED if opcode is not handled
	 * - TCMU_ASYNC_HANDLED if opcode is handled asynchronously
	 *
	 * Handlers that set nr_threads > 0 and async handlers
	 * that implement handle_cmd and the IO callouts below return:
	 *
	 * 0 if the handler has queued the command.
	 * - TCMU_NOT_HANDLED if the command is not supported.
	 * - SAM_STAT_TASK_SET_FULL if the handler was not able to allocate
	 *   resources for the command.
	 *
	 * If 0 is returned the handler must call the tcmulib_cmd->done
	 * function with SAM_STAT_GOOD or a SAM status code and set the
	 * the sense asc/ascq if needed.
	 */
	handle_cmd_fn_t handle_cmd;

	/*
	 * Below callbacks are only exected called by generic_handle_cmd.
	 * Returns:
	 * - 0 if the handler has queued the command.
	 * - SAM_STAT_TASK_SET_FULL if the handler was not able to allocate
	 *   resources for the command.
	 *
	 * If 0 is returned the handler must call the tcmulib_cmd->done
	 * function with SAM_STAT_GOOD or a SAM status code and set the
	 * the sense asc/ascq if needed.
	 */
	rw_fn_t write;
	rw_fn_t read;
	flush_fn_t flush;
};

/*
 * Each tcmu-runner (tcmur) handler plugin must export the
 * following. It usually just calls tcmur_register_handler.
 */
void tcmur_handler_init(void);

/*
 * APIs for tcmur only
 */
int tcmur_register_handler(struct tcmur_handler *handler);
bool tcmur_unregister_handler(struct tcmur_handler *handler);

#ifdef __cplusplus
}
#endif

#endif
