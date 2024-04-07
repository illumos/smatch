/*
 * Copyright (C) 2009 Dan Carpenter.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see http://www.gnu.org/copyleft/gpl.txt
 */

#include "smatch.h"
#include "smatch_slist.h"
#include "smatch_extra.h"

static int my_id;

#define	KM_SLEEP	0x0000

STATE(derefed);
STATE(ok);

static void deref_hook(struct expression *expr)
{
	struct sm_state *sm, *tmp;

	sm = get_sm_state_expr(my_id, expr);
	if (sm) {
		FOR_EACH_PTR(sm->possible, tmp) {
			if (tmp->state == &ok)
				return;
		} END_FOR_EACH_PTR(tmp);
	}
	if (implied_not_equal(expr, 0))
		return;
	if (is_impossible_path())
		return;

	set_state_expr(my_id, expr, &derefed);
}

static void match_condition(struct expression *expr)
{
	struct sm_state *sm;
	char *name;

	if (__in_pre_condition)
		return;

	name = get_macro_name(expr->pos);
	if (name &&
	    (strcmp(name, "likely") != 0 && strcmp(name, "unlikely") != 0))
		return;

	if (!is_pointer(expr))
		return;

	sm = get_sm_state_expr(my_id, expr);
	if (!sm || sm->state != &derefed)
		return;

	sm_warning("variable dereferenced before check '%s' (see line %d)", sm->name, sm->line);
	set_state_expr(my_id, expr, &undefined);
}

static void match_err_check(struct expression *expr, const char *name, struct symbol *sym, void *data)
{
	struct sm_state *sm;
	char *macro;

	if (__in_pre_condition)
		return;

	sm = get_sm_state(my_id, name, sym);
	if (!sm || sm->state != &derefed)
		return;

	macro = get_macro_name(expr->pos);
	if (macro && strcmp(macro, "gvt_vgpu_err") == 0)
		return;

	sm_warning("variable dereferenced before IS_ERR check '%s' (see line %d)", sm->name, sm->line);
	set_state_expr(my_id, expr, &undefined);
}

static int called_with_no_fail(struct expression *call, int param)
{
	struct expression *arg;
	sval_t sval;

	if (param == -1)
		return 0;
	call = strip_expr(call);
	if (call->type != EXPR_CALL)
		return 0;
	arg = get_argument_from_call_expr(call->args, param);
	if (get_value(arg, &sval)) {
		if (option_project == PROJ_ILLUMOS_KERNEL &&
		    sval.uvalue == KM_SLEEP)
			return 1;
	}
	return 0;
}

static void match_assign_returns_null(const char *fn, struct expression *expr, void *_gfp)
{
	int gfp_param = PTR_INT(_gfp);

	/* set state ok if our allocation will never be NULL */
	if (called_with_no_fail(expr->right, gfp_param))
		set_state_expr(my_id, expr->left, &ok);
}

static void register_allocation_funcs(char *name)
{
	struct token *token;
	const char *func;
	int arg;

	token = get_tokens_file(name);
	if (!token)
		return;
	if (token_type(token) != TOKEN_STREAMBEGIN)
		return;
	token = token->next;
	while (token_type(token) != TOKEN_STREAMEND) {
		if (token_type(token) != TOKEN_IDENT)
			return;
		func = show_ident(token->ident);
		token = token->next;
		if (token_type(token) == TOKEN_IDENT)
			arg = -1;
		else if (token_type(token) == TOKEN_NUMBER)
			arg = atoi(token->number);
		else
			return;
		add_function_assign_hook(func, &match_assign_returns_null, INT_PTR(arg));
		token = token->next;
	}
	clear_token_alloc();
}

void check_deref_check(int id)
{
	my_id = id;

	add_dereference_hook(deref_hook);
	add_modification_hook(my_id, &set_undefined);

	add_hook(&match_condition, CONDITION_HOOK);

	add_function_param_key_hook("IS_ERR", &match_err_check, 0, "$", NULL);
	add_function_param_key_hook("PTR_ERR_OR_ZERO", &match_err_check, 0, "$", NULL);
	add_function_param_key_hook("IS_ERR_OR_NULL", &match_err_check, 0, "$", NULL);
	if (option_project == PROJ_ILLUMOS_KERNEL)
		register_allocation_funcs("illumos_kernel.allocation_funcs_gfp");
}
