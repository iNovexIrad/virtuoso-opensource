/*
 *  $Id$
 *
 *  This file is part of the OpenLink Software Virtuoso Open-Source (VOS)
 *  project.
 *
 *  Copyright (C) 1998-2006 OpenLink Software
 *
 *  This project is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; only version 2 of the License, dated June 1991.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 *  
*/
#include "sparql2sql.h"
#include "sqlparext.h"
#include "arith.h"
#include "sqlcmps.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "sparql_p.h"
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
#include "xmlparser.h"
#include "xmlparser_impl.h"
#ifdef __cplusplus
}
#endif
#include "xml_ecm.h"

/* PART 1. EXPRESSION TERM REWRITING */

/* Composing list of retvals instead of '*'.
\c trav_env_this is not used.
\c common_env points to dk_set_t of collected distinct variable names. */
int
sparp_gp_trav_list_expn_retvals (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  caddr_t varname;
  if (SPAR_VARIABLE != curr->type)
    return 0;
  varname = curr->_.var.vname;
  if (SPART_VARNAME_IS_GLOB(varname)) /* Query run-time env or external query param ? -- not in result-set */
    return SPAR_GPT_NODOWN;
  DO_SET (caddr_t, listed, (dk_set_t *)(common_env))
    {
      if (!strcmp (listed, varname))
        return SPAR_GPT_NODOWN;
    }
  END_DO_SET()
  t_set_push ((dk_set_t *)(common_env), varname);
  return SPAR_GPT_NODOWN;
}

int
sparp_gp_trav_list_subquery_retvals (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  int ctr;
  if (SPAR_GP != curr->type)
    return 0;
  if (SELECT_L != curr->_.gp.subtype)
    return 0;
  DO_BOX_FAST (SPART *, retval, ctr, curr->_.gp.subquery->_.req_top.retvals)
    {
      caddr_t name;
      switch (SPART_TYPE (retval))
        {
          case SPAR_VARIABLE: name = retval->_.var.vname; break;
          case SPAR_ALIAS: name = retval->_.alias.aname; break;
          default: name = NULL;
        }
      if (NULL == name)
        goto ignore_retval_name; /* see below */
      if (SPART_VARNAME_IS_GLOB(name))
        goto ignore_retval_name; /* see below */
      DO_SET (caddr_t, listed, (dk_set_t *)(common_env))
        {
          if (!strcmp (listed, name))
            goto ignore_retval_name; /* see below */
        }
      END_DO_SET()
      t_set_push ((dk_set_t *)(common_env), name);
ignore_retval_name: ;
    }
  END_DO_BOX_FAST;
  return 0;
}

int sparp_gp_trav_list_nonaggregate_retvals (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  caddr_t varname;
  if ((SPAR_FUNCALL == curr->type) && curr->_.funcall.agg_mode)
    return SPAR_GPT_NODOWN;
  if (SPAR_VARIABLE != curr->type) /* Not a variable ? -- nothing to do */
    return SPAR_GPT_ENV_PUSH; /* To preserve sts_this->sts_curr_array and sts_this->sts_ofs_of_curr_in_array for wrapper of vars into fake MAX() */
  varname = curr->_.var.vname;
  if (SPART_VARNAME_IS_GLOB(varname)) /* Query run-time env or external query param ? -- not in result-set */
    return SPAR_GPT_NODOWN;
  DO_SET (caddr_t, listed, (dk_set_t *)(common_env))
    {
      if (!strcmp (listed, varname))
        return SPAR_GPT_NODOWN;
    }
  END_DO_SET()
  t_set_push ((dk_set_t *)(common_env), varname);
  return SPAR_GPT_NODOWN;
}

void
sparp_expand_top_retvals (sparp_t *sparp, SPART *query, int safely_copy_all_vars)
{
  sparp_env_t *env = sparp->sparp_env;
  caddr_t retselid = query->_.req_top.retselid;
  dk_set_t names = NULL;
  dk_set_t new_vars = NULL;
  SPART **retvals = query->_.req_top.retvals;
  if (IS_BOX_POINTER (retvals))
    {
      if (safely_copy_all_vars)
        query->_.req_top.orig_retvals = sparp_treelist_full_copy (sparp, retvals, query->_.req_top.pattern); /* No clonig equivs here but no equivs at this moment at all */
      else
        query->_.req_top.orig_retvals = (SPART **) t_box_copy ((box_t) retvals);
      if (0 == sparp->sparp_query_uses_aggregates)
    return;
      sparp_gp_localtrav_treelist (sparp, retvals,
        NULL, &names,
            NULL, NULL,
        sparp_gp_trav_list_nonaggregate_retvals, NULL, NULL,
        NULL );
      sparp_gp_localtrav_treelist (sparp, query->_.req_top.order,
        NULL, &names,
        NULL, NULL,
        sparp_gp_trav_list_nonaggregate_retvals, NULL, NULL,
            NULL );
      if (0 != BOX_ELEMENTS_0 (query->_.req_top.groupings))
        {
          dk_set_t names_in_groupings = NULL;
          sparp_gp_localtrav_treelist (sparp, query->_.req_top.groupings,
            NULL, &names_in_groupings,
            sparp_gp_trav_list_subquery_retvals, NULL,
            sparp_gp_trav_list_expn_retvals, NULL, NULL,
            NULL );
          while (NULL != names)
            {
              caddr_t varname = t_set_pop (&names);
              if (0 > dk_set_position_of_string (names_in_groupings, varname))
                spar_error (sparp, "Variable ?%.200s is used in the result set outside aggregate and not mentioned in GROUP BY clause", varname);
            }
          return;
        }
      t_set_push (&(env->spare_selids), retselid);
      while (NULL != names)
        {
          caddr_t varname = t_set_pop (&names);
          SPART *var = spar_make_variable (sparp, varname);
          t_set_push (&new_vars, var);
        }  
      t_set_pop (&(env->spare_selids));
      query->_.req_top.groupings = (SPART **)t_revlist_to_array (new_vars);
      return;
    }
  {
    sparp_trav_state_t stss [SPARP_MAX_SYNTDEPTH+2];
    memset (stss, 0, sizeof (sparp_trav_state_t) * (SPARP_MAX_SYNTDEPTH+2));
    sparp_gp_trav_int (sparp, query->_.req_top.pattern, stss+1, &names,
      sparp_gp_trav_list_subquery_retvals, NULL,
      sparp_gp_trav_list_expn_retvals, NULL, NULL,
    NULL );
  }
  t_set_push (&(env->spare_selids), retselid);
  while (NULL != names)
    {
      caddr_t varname = t_set_pop (&names);
      SPART *var = spar_make_variable (sparp, varname);
      t_set_push (&new_vars, var);
    }  
  t_set_pop (&(env->spare_selids));
  if ((SPART **)_STAR == retvals)
    {
      if (NULL == new_vars)
        {
          if (env->spare_signal_void_variables)
            spar_error (sparp, "The list of return values contains '*' but the pattern does not contain variables");
          else
            t_set_push (&new_vars, spartlist (sparp, 4, SPAR_ALIAS,
              t_box_num (1), t_box_dv_short_string ("_star_fake"), SSG_VALMODE_AUTO ) );
        }
      query->_.req_top.retvals = retvals = (SPART **)t_list_to_array (new_vars);
      if (safely_copy_all_vars)
        query->_.req_top.orig_retvals = sparp_treelist_full_copy (sparp, retvals, query->_.req_top.pattern); /* No clonig equivs here but no equivs at this moment at all */
      else
        query->_.req_top.orig_retvals = (SPART **) t_box_copy ((box_t) retvals);
    }
/*  else if ((SPART **)COUNT_L == old_retvals)
    {
      SPART *countagg;
      t_set_push (&new_vars, (caddr_t)((ptrlong)1));
      countagg = spar_make_funcall (sparp, 1, "bif:COUNT", (SPART **)t_list_to_array (new_vars));
      sparp->sparp_expr->_.req_top.retvals = (SPART **)t_list (1, countagg);
    }*/
  else
    spar_internal_error (sparp, "sparp_" "expand_top_retvals () failed to process special result-set");
}

int sparp_gp_trav_wrap_vars_in_max (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  caddr_t varname;
  ssg_valmode_t native;
  if ((SPAR_FUNCALL == curr->type) && curr->_.funcall.agg_mode)
    return SPAR_GPT_NODOWN;
  if (SPAR_VARIABLE != curr->type) /* Not a variable ? -- nothing to do */
    return SPAR_GPT_ENV_PUSH; /* To preserve sts_this->sts_curr_array and sts_this->sts_ofs_of_curr_in_array for wrapper of vars into fake MAX() */
  varname = curr->_.var.vname;
  if (SPART_VARNAME_IS_GLOB(varname)) /* Query run-time env or external query param ? -- not in result-set */
    return SPAR_GPT_NODOWN;
  native = sparp_expn_native_valmode (sparp, curr);
  if (IS_BOX_POINTER (native) && native->qmfIsBijection)
    return SPAR_GPT_NODOWN;
  sts_this->sts_curr_array[sts_this->sts_ofs_of_curr_in_array] =
    spartlist (sparp, 4, SPAR_ALIAS,
      spar_make_funcall (sparp, 1, t_box_dv_uname_string ("SPECIAL::bif:MAX"), (SPART **)t_list (1, curr)),
      varname, SSG_VALMODE_AUTO );
  return SPAR_GPT_NODOWN;
}

void
sparp_wpar_retvars_in_max (sparp_t *sparp, SPART *query)
{
  SPART **retvals = query->_.req_top.retvals;
  caddr_t retvalmode_name, formatmode_name;
  if (0 == sparp->sparp_query_uses_aggregates)
    return;
  retvalmode_name = query->_.req_top.retvalmode_name;
  formatmode_name = query->_.req_top.formatmode_name;
  if (((SELECT_L == query->_.req_top.subtype) ||
    (DISTINCT_L == query->_.req_top.subtype) ) &&
    (NULL == formatmode_name) &&
    ((NULL == retvalmode_name) ||
    (SSG_VALMODE_SQLVAL == ssg_find_valmode_by_name (retvalmode_name)) ) )
  return;
  sparp_gp_localtrav_treelist (sparp, retvals,
    NULL, NULL,
    NULL, NULL,
    sparp_gp_trav_wrap_vars_in_max, NULL, NULL,
    NULL );
}

int
sparp_gp_trav_preopt_in_gp (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  switch (SPART_TYPE(curr))
    {
    case SPAR_GP:
      if (SELECT_L == curr->_.gp.subtype)
        {
          sparp_t *sub_sparp = sparp_down_to_sub (sparp, curr);
          sparp_rewrite_retvals (sub_sparp, 1);
          sparp_up_from_sub (sparp, curr, sub_sparp);
        }
      return SPAR_GPT_ENV_PUSH;
    }
  return 0;
}

int
sparp_gp_trav_preopt_expn_subq (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  sparp_t *sub_sparp = sparp_down_to_sub (sparp, curr);
  sparp_rewrite_retvals (sub_sparp, 1);
  sparp_up_from_sub (sparp, curr, sub_sparp);
  return 0;
}

void
sparp_rewrite_retvals (sparp_t *sparp, int safely_copy_retvals)
{
  rdf_grab_config_t *rgc = &(sparp->sparp_env->spare_grab);
  SPART *root = sparp->sparp_expr;
  if (rgc->rgc_all)
    spar_add_rgc_vars_and_consts_from_retvals (sparp, root->_.req_top.retvals);
  if (safely_copy_retvals)
    root->_.req_top.expanded_orig_retvals = sparp_treelist_full_copy (sparp, root->_.req_top.retvals, root->_.req_top.pattern);
  else
    root->_.req_top.expanded_orig_retvals = (SPART **)t_box_copy ((caddr_t)(root->_.req_top.retvals));
/* Unlike spar_retvals_of_construct() that can be called during parsing,
spar_retvals_of_describe() should wait for obtaining all variables and then
sparp_expand_top_retvals () to process 'DESCRIBE * ...'. */
  if (DESCRIBE_L == root->_.req_top.subtype)
    root->_.req_top.retvals = 
      spar_retvals_of_describe (sparp,
        root->_.req_top.retvals,
        root->_.req_top.limit,
        root->_.req_top.offset );
  sparp_gp_trav (sparp, sparp->sparp_expr->_.req_top.pattern, NULL,
    sparp_gp_trav_preopt_in_gp, NULL,
    NULL, NULL, sparp_gp_trav_preopt_expn_subq,
    NULL );
}

/* Composing counters of usages.
\c trav_env_this points to the innermost graph pattern.
\c common_env is not used. */

int
sparp_gp_trav_cu_in_triples (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  int fctr;
  SPART *gp = sts_this->sts_ancestor_gp;
  switch (SPART_TYPE(curr))
    {
    case SPAR_GP:
      if ((SELECT_L == curr->_.gp.subtype) && (0 == curr->_.gp.equiv_count))
        {
          int ctr;
          sparp_t *sub_sparp = sparp_down_to_sub (sparp, curr);
          sparp_rewrite_all (sub_sparp, 1);
          sparp_up_from_sub (sparp, curr, sub_sparp);
          DO_BOX_FAST (SPART *, retval, ctr, curr->_.gp.subquery->_.req_top.orig_retvals)
            {
              caddr_t name;
              switch (SPART_TYPE (retval))
                {
                  case SPAR_VARIABLE:
                    name = retval->_.var.vname;
                    curr->_.gp.subquery->_.req_top.orig_retvals[ctr] = 
                      retval = spartlist (sparp, 4, SPAR_ALIAS, retval, name, SSG_VALMODE_AUTO);
                    break;
                  case SPAR_ALIAS:
                    name = retval->_.alias.aname; break;
                  default: name = NULL;
                }
              if (NULL == name)
                goto ignore_retval_name; /* see below */
              if (SPART_VARNAME_IS_GLOB(name))
                goto ignore_retval_name; /* see below */
              sparp_equiv_get (sparp, curr, (SPART *)name, SPARP_EQUIV_INS_CLASS | SPARP_EQUIV_GET_NAMESAKES | SPARP_EQUIV_ADD_SUBQUERY_USE);
ignore_retval_name: ;
            }
          END_DO_BOX_FAST;
        }
      return SPAR_GPT_ENV_PUSH;
    case SPAR_TRIPLE: break;
    default: return 0;
    }
  for (fctr = 0; fctr < SPART_TRIPLE_FIELDS_COUNT; fctr++)
    {
      SPART *fld = curr->_.triple.tr_fields[fctr];
      sparp_equiv_t *eq;
      switch (SPART_TYPE(fld))
        {
	case SPAR_VARIABLE: case SPAR_BLANK_NODE_LABEL: break;
	default: continue;
	}
      eq = sparp_equiv_get (sparp, gp, fld, SPARP_EQUIV_INS_CLASS | SPARP_EQUIV_INS_VARIABLE | SPARP_EQUIV_ADD_GPSO_USE);
      if (UNION_L != gp->_.gp.subtype)
        sparp_rvr_tighten (sparp, &(eq->e_rvr), &(fld->_.var.rvr), ~0);
    }
  if (UNION_L == gp->_.gp.subtype)
    {
      int eq_ctr;
      SPARP_FOREACH_GP_EQUIV (sparp, gp, eq_ctr, eq)
        {
          if ((0 < eq->e_gspo_uses) && (0 == eq->e_rvr.rvrRestrictions))
            {
	      int varctr;
              rdf_val_range_t acc;
              memset (&acc, 0, sizeof (rdf_val_range_t));
              acc.rvrRestrictions = SPART_VARR_CONFLICT;
              for (varctr = eq->e_var_count; varctr--; /*no step*/)
                sparp_rvr_loose (sparp, &acc, &(eq->e_vars[varctr]->_.var.rvr), ~SPART_VARR_NOT_NULL);
              sparp_rvr_tighten (sparp, &(eq->e_rvr), &acc, ~0);              
            }
        }
      END_SPARP_FOREACH_GP_EQUIV;
    }
  return SPAR_GPT_NODOWN;
}

int
sparp_gp_trav_cu_out_triples_1 (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  if (SPAR_GP == SPART_TYPE(curr))
    {
      int eq_ctr;
      SPARP_FOREACH_GP_EQUIV (sparp, curr, eq_ctr, eq)
        {
          int sub_ctr;
          eq->e_nested_bindings = 0;
          DO_BOX_FAST_REV (ptrlong, sub_idx, sub_ctr, eq->e_subvalue_idxs)
            {
              sparp_equiv_t *sub_eq = SPARP_EQUIV(sparp,sub_idx);
              if ((0 < sub_eq->e_gspo_uses) || (0 < sub_eq->e_subquery_uses) || (0 < sub_eq->e_nested_bindings))
                eq->e_nested_bindings += 1;
            }
          END_DO_BOX_FAST;
        }
      END_SPARP_FOREACH_GP_EQUIV;
    }
  return 0;
}

int
sparp_gp_trav_cu_out_triples_2 (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  if (SPAR_GP == SPART_TYPE(curr))
    {
      int eq_ctr;
      SPARP_FOREACH_GP_EQUIV (sparp, curr, eq_ctr, eq)
        {
          int varnamectr;
          DO_BOX_FAST (caddr_t, varname, varnamectr, eq->e_varnames)
            {
              t_set_push_new_string ((dk_set_t *)common_env, varname);
            }
          END_DO_BOX_FAST;
        }
      END_SPARP_FOREACH_GP_EQUIV;
    }
  return 0;
}

int
sparp_gp_trav_cu_in_expns (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  SPART *gp = sts_this->sts_ancestor_gp;
  sparp_equiv_t *eq;
  switch (SPART_TYPE(curr))
    {
    case SPAR_VARIABLE: break;
    case SPAR_BLANK_NODE_LABEL: break;
    default: return 0;
    }
  eq = sparp_equiv_get (sparp, gp, curr, SPARP_EQUIV_INS_CLASS | SPARP_EQUIV_INS_VARIABLE | SPARP_EQUIV_ADD_CONST_READ);
  sparp_rvr_tighten (sparp, &(eq->e_rvr), &(curr->_.var.rvr), ~0);
  return 0;
}

int
sparp_gp_trav_cu_in_subq (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  dk_set_t all_sub_vars = NULL;
  sparp_t *sub_sparp;
  sub_sparp = sparp_down_to_sub (sparp, curr);
  sparp_count_usages (sub_sparp, &all_sub_vars);
  sparp_up_from_sub (sparp, curr, sub_sparp);
  DO_SET (caddr_t, varname, &all_sub_vars)
    {
      sparp_equiv_get (sparp, curr, (SPART *)varname, SPARP_EQUIV_INS_CLASS | SPARP_EQUIV_GET_NAMESAKES | SPARP_EQUIV_ADD_OPTIONAL_READ);
      if (NULL != sts_this->sts_ancestor_gp)
        sparp_equiv_get (sparp, sts_this->sts_ancestor_gp, (SPART *)varname, SPARP_EQUIV_INS_CLASS | SPARP_EQUIV_GET_NAMESAKES | SPARP_EQUIV_ADD_OPTIONAL_READ);
    }
  END_DO_SET ()
  return 0;
}

int
sparp_gp_trav_cu_in_retvals (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  SPART *top_gp = (SPART *)(common_env);
  sparp_equiv_t *eq;
  switch (SPART_TYPE(curr))
    {
    case SPAR_VARIABLE: break;
    case SPAR_BLANK_NODE_LABEL: break;
    default: return 0;
    }
  curr->_.var.selid = top_gp->_.gp.selid;
  curr->_.var.tabid = NULL;
  eq = sparp_equiv_get (sparp, top_gp, curr, SPARP_EQUIV_INS_CLASS | SPARP_EQUIV_INS_VARIABLE | SPARP_EQUIV_ADD_CONST_READ);
  curr->_.var.equiv_idx = eq->e_own_idx;
  curr->_.var.rvr.rvrRestrictions |= SPART_VARR_EXPORTED;
  sparp_equiv_tighten (sparp, eq, &(curr->_.var.rvr), ~0);
  return 0;
}

void
sparp_count_usages (sparp_t *sparp, dk_set_t *optvars_ret)
{
  sparp_gp_trav (sparp, sparp->sparp_expr->_.req_top.pattern, optvars_ret,
    sparp_gp_trav_cu_in_triples, sparp_gp_trav_cu_out_triples_1,
    sparp_gp_trav_cu_in_expns, NULL, sparp_gp_trav_cu_in_subq,
        NULL );
  sparp_trav_out_clauses (sparp, sparp->sparp_expr, sparp->sparp_expr->_.req_top.pattern,
        NULL, NULL,
        sparp_gp_trav_cu_in_retvals, NULL, sparp_gp_trav_cu_in_subq,
        NULL );
  if (NULL != optvars_ret)
    sparp_gp_trav (sparp, sparp->sparp_expr->_.req_top.pattern, optvars_ret,
      NULL, sparp_gp_trav_cu_out_triples_2,
      NULL, NULL, sparp_gp_trav_cu_in_subq,
        NULL );
}

int
sparp_tree_returns_ref (sparp_t *sparp, SPART *tree)
{
  switch (SPART_TYPE (tree))
    {
    case SPAR_QNAME: return 1;
    case SPAR_BUILT_IN_CALL:
      if (IRI_L == tree->_.builtin.btype)
        return 1;
      break;
    }
  return 0;
}

static int
sparp_expn_rside_rank (SPART *tree)
{
  switch (SPART_TYPE (tree))
    {
    case SPAR_VARIABLE: case SPAR_BLANK_NODE_LABEL:
      {
        ptrlong restr = tree->_.var.rvr.rvrRestrictions;
        if (SPART_VARR_FIXED & restr)
          return 0x20;
        if ((SPART_VARR_GLOBAL | SPART_VARR_EXTERNAL) & restr)
          return 0x10;
        if (SPART_VARR_IS_IRI & restr)
          return 0x8;
        if (SPART_VARR_IS_REF & restr)
          return 0x4;
        if (SPART_VARR_IS_LIT & restr)
          return 0x2;
        return 0x0;
      }
    case SPAR_LIT: return 0x40;
    case SPAR_QNAME: return 0x100;
    case SPAR_BUILT_IN_CALL: return 0x1000;
    case SPAR_FUNCALL: return 0x10000;
    }
  return 0x20000;
}

void
sparp_rotate_comparisons_by_rank (SPART *filt)
{
  switch (SPART_TYPE (filt))
    {
    case BOP_EQ:
    case BOP_NEQ:
    case BOP_LT:
    case BOP_LTE:
    case BOP_GT:
    case BOP_GTE:
      {
        SPART *l = filt->_.bin_exp.left;
        SPART *r = filt->_.bin_exp.right;
        int lrrank = sparp_expn_rside_rank (l);
        int rrrank = sparp_expn_rside_rank (r);
        if (lrrank > rrrank)
          {
            filt->_.bin_exp.right = l;
            filt->_.bin_exp.left = r;
            switch (SPART_TYPE (filt))
              {
              case BOP_LT:	filt->type = BOP_GT; break;
              case BOP_LTE:	filt->type = BOP_GTE; break;
              case BOP_GT:	filt->type = BOP_LT; break;
              case BOP_GTE:	filt->type = BOP_LTE; break;
              }
          }
        break;
      }
    case SPAR_BUILT_IN_CALL:
      if (SAMETERM_L == filt->_.builtin.btype)
        {
          SPART *l = filt->_.builtin.args[0];
          SPART *r = filt->_.builtin.args[1];
          int lrrank = sparp_expn_rside_rank (l);
          int rrrank = sparp_expn_rside_rank (r);
          if (lrrank > rrrank)
            {
              filt->_.builtin.args[0] = r;
              filt->_.builtin.args[1] = l;
            }
        }
      break;
    default:
      break;
    }
}

typedef struct so_BOP_OR_filter_ctx_s
{
  SPART *bofc_var_sample;	/*!< Common optimizable variable in question, set to (ptrlong)1 if not common or not optimizable or global */
  dk_set_t bofc_strings;	/*!< Collected string values, they may be convert into sprintf format strings to tighten equiv of the common variable */
  ptrlong bofc_can_be_iri;	/*!< Flag if there's at least equality to the IRI */
  ptrlong bofc_can_be_literal;	/*!< Flag if there's at least equality to the literal string */
} so_BOP_OR_filter_ctx_t;

int
sparp_optimize_BOP_OR_filter_walk_lvar (SPART *lvar, so_BOP_OR_filter_ctx_t *ctx)
{
  if (SPAR_VARIABLE != SPART_TYPE (lvar))
    return 1; /* for optimization, there should be variable at left */
  if (NULL == ctx->bofc_var_sample)
    ctx->bofc_var_sample = lvar;
  else if (strcmp (ctx->bofc_var_sample->_.var.vname, lvar->_.var.vname))
    {
      ctx->bofc_var_sample = (SPART *)((ptrlong)1);
      return 1; /* for optimization, there should be _same_ variable at left */
    }
  return 0;
}

int
sparp_optimize_BOP_OR_filter_walk_rexpn (SPART *rexpn, so_BOP_OR_filter_ctx_t *ctx)
{
  if (SPAR_QNAME == SPART_TYPE (rexpn))
    {
      ctx->bofc_can_be_iri++;
      dk_set_push (&(ctx->bofc_strings), rexpn->_.lit.val);
      return 0;
    }
  if (SPAR_LIT == SPART_TYPE (rexpn))
    {
      caddr_t lit_val = rexpn->_.lit.val;
      if (!IS_STRING_DTP (DV_TYPE_OF (lit_val)))
        return 1;
      ctx->bofc_can_be_literal++;
      dk_set_push (&(ctx->bofc_strings), lit_val);
      return 0;
    }
  return 1;
}

int
sparp_optimize_BOP_OR_filter_walk (SPART *filt, so_BOP_OR_filter_ctx_t *ctx)
{
  ptrlong filt_type = SPART_TYPE (filt);
  switch (filt_type)
    {
    case BOP_OR:
      if (sparp_optimize_BOP_OR_filter_walk (filt->_.bin_exp.left, ctx))
        return 1;
      if (sparp_optimize_BOP_OR_filter_walk (filt->_.bin_exp.right, ctx))
        return 1;
      return 0;
    case SPAR_BUILT_IN_CALL:
      if (IN_L == filt->_.builtin.btype)
        {
          int argctr;
          if (sparp_optimize_BOP_OR_filter_walk_lvar (filt->_.builtin.args[0], ctx))
            goto cannot_optimize; /* see below */
          for (argctr = BOX_ELEMENTS (filt->_.builtin.args); 0 < --argctr; /* no step */)
            {
              if (sparp_optimize_BOP_OR_filter_walk_rexpn (filt->_.builtin.args[argctr], ctx))
                goto cannot_optimize; /* see below */
            }
          return 0;
        }
      if (SAMETERM_L != filt->_.builtin.btype)
        goto cannot_optimize; /* see below */
      /* no break, try get optimization hints like it is BOP_EQ */
    case BOP_EQ: /* No break */
      sparp_rotate_comparisons_by_rank (filt);
      if (sparp_optimize_BOP_OR_filter_walk_lvar (filt->_.bin_exp.left, ctx))
        goto cannot_optimize; /* see below */
      if (sparp_optimize_BOP_OR_filter_walk_rexpn (filt->_.bin_exp.right, ctx))
        goto cannot_optimize; /* see below */
      return 0;
    default: ;
    }
cannot_optimize:
/* The very natural default is to say 'cannot optimize' and escape */
  ctx->bofc_var_sample = (SPART*)(ptrlong)1;
  return 1;
}

/* Processing of simple filters inside BOP_OR (or top-level IN_L) that introduce restrictions on variables. */
int
sparp_optimize_BOP_OR_filter (sparp_t *sparp, SPART *curr, SPART *filt)
{
  sparp_equiv_t *eq_l;
  rdf_val_range_t new_rvr;
  so_BOP_OR_filter_ctx_t ctx;
  int sff_ctr;
  memset (&ctx, 0, sizeof (so_BOP_OR_filter_ctx_t));
  if (sparp_optimize_BOP_OR_filter_walk (filt, &ctx))
    {
      while (NULL != ctx.bofc_strings) dk_set_pop (&(ctx.bofc_strings));
      return 0;
    }
  eq_l = sparp_equiv_get (sparp, curr, ctx.bofc_var_sample, 0);
  memset (&new_rvr, 0, sizeof (rdf_val_range_t));
  if (0 == ctx.bofc_can_be_iri)
    new_rvr.rvrRestrictions |= SPART_VARR_IS_LIT;
  if (0 == ctx.bofc_can_be_literal)
    new_rvr.rvrRestrictions |= SPART_VARR_IS_REF | SPART_VARR_IS_IRI;
  new_rvr.rvrRestrictions |= SPART_VARR_NOT_NULL | SPART_VARR_SPRINTFF;
  new_rvr.rvrSprintffCount = dk_set_length (ctx.bofc_strings);
  new_rvr.rvrSprintffs = (ccaddr_t *)t_alloc_box (DV_ARRAY_OF_POINTER, new_rvr.rvrSprintffCount * sizeof(caddr_t));
  for (sff_ctr = new_rvr.rvrSprintffCount; sff_ctr--; /* no step */)
    new_rvr.rvrSprintffs[sff_ctr] = sprintff_from_strg (dk_set_pop (&(ctx.bofc_strings)), 1);
  sparp_equiv_tighten (sparp, eq_l, &new_rvr, ~0);
/* TBD: it is possible to remove branches of OR that contradicts with known restrictions of \c eq_l */
  return 0;
}

/* For an equality in group \c curr between member of \c eq_l and expression \c r,
the function restricts \c eq_l or even merges it with variable of other equiv.
\returns 1 if equality is no longer needed due to merge, 0 otherwise */
int
spar_var_eq_to_equiv (sparp_t *sparp, SPART *curr, sparp_equiv_t *eq_l, SPART *r)
{
  int ret = 0;
  ptrlong tree_restr_bits = sparp_restr_bits_of_expn (sparp, r);
  eq_l->e_rvr.rvrRestrictions |= (tree_restr_bits & (
    SPART_VARR_IS_REF | SPART_VARR_IS_IRI | SPART_VARR_IS_BLANK |
    SPART_VARR_IS_LIT | SPART_VARR_LONG_EQ_SQL |
    SPART_VARR_NOT_NULL | SPART_VARR_ALWAYS_NULL ) );
  switch (SPART_TYPE (r))
    {
    case SPAR_VARIABLE: case SPAR_BLANK_NODE_LABEL:
      {
        sparp_equiv_t *eq_r = sparp_equiv_get (sparp, curr, r, 0);
        eq_l->e_rvr.rvrRestrictions |= SPART_VARR_NOT_NULL;
        ret = sparp_equiv_merge (sparp, eq_l, eq_r);
        break;
      }
    case SPAR_LIT: case SPAR_QNAME:
      {
        ret = sparp_equiv_restrict_by_constant (sparp, eq_l, NULL, r);
        break;
      }
    case SPAR_BUILT_IN_CALL:
      {
        switch (r->_.builtin.btype)
          {
          case STR_L: eq_l->e_rvr.rvrRestrictions |= SPART_VARR_IS_LIT | SPART_VARR_NOT_NULL; break;
          case IRI_L: eq_l->e_rvr.rvrRestrictions |= SPART_VARR_IS_REF | SPART_VARR_NOT_NULL; break;
          }
        break;
      }
    }
  if (
    (SPARP_EQUIV_MERGE_OK == ret) ||
    (SPARP_EQUIV_MERGE_CONFLICT == ret) ||
    (SPARP_EQUIV_MERGE_DUPE == ret) )
    return 1;
  return 0;
}

/* Processing of simple filters that introduce restrictions on variables
\c trav_env_this is not used.
\c common_env is not used. */

int
sparp_filter_to_equiv (sparp_t *sparp, SPART *curr, SPART *filt)
{
/* We rotate comparisons before anything else */
  sparp_rotate_comparisons_by_rank (filt);
/* Now filters can be processed */
  switch (SPART_TYPE (filt))
    {
    case BOP_EQ:
      {
        SPART *l = filt->_.bin_exp.left;
        SPART *r = filt->_.bin_exp.right;
        switch (SPART_TYPE (l))
          {
          case SPAR_VARIABLE: case SPAR_BLANK_NODE_LABEL:
          {
		sparp_equiv_t *eq_l = sparp_equiv_get (sparp, curr, l, 0);
            return spar_var_eq_to_equiv (sparp, curr, eq_l, r);
	      }
          case SPAR_QNAME:
            {
              caddr_t lval = l->_.lit.val;
              if ((SPAR_BUILT_IN_CALL == SPART_TYPE (r)) &&
                (DATATYPE_L == r->_.builtin.btype) )
                {
                  SPART *rarg1 = r->_.builtin.args[0];
	          if (SPAR_IS_BLANK_OR_VAR (rarg1))
                    {
                      sparp_equiv_t *rarg1_eq = sparp_equiv_get (sparp, curr, rarg1, 0);
                      rarg1_eq->e_rvr.rvrRestrictions |= SPART_VARR_NOT_NULL;
                      if (
                        (lval == uname_xmlschema_ns_uri_hash_boolean) ||
                        (lval == uname_xmlschema_ns_uri_hash_string) )
                        {
                          rarg1_eq->e_rvr.rvrRestrictions |= SPART_VARR_IS_LIT;
                        }
                      else if (
                        (lval == uname_xmlschema_ns_uri_hash_date) ||
                        (lval == uname_xmlschema_ns_uri_hash_dateTime) ||
                        (lval == uname_xmlschema_ns_uri_hash_decimal) ||
                        (lval == uname_xmlschema_ns_uri_hash_double) ||
                        (lval == uname_xmlschema_ns_uri_hash_float) ||
                        (lval == uname_xmlschema_ns_uri_hash_integer) ||
                        (lval == uname_xmlschema_ns_uri_hash_time) )
                        {
                          rarg1_eq->e_rvr.rvrRestrictions |= SPART_VARR_IS_LIT | SPART_VARR_LONG_EQ_SQL;
                        }
                    }
                  return 0;
                }
            }
          }
        break;
     }
    case BOP_NOT: break;
    case SPAR_BUILT_IN_CALL:
      {
        SPART *arg1 = filt->_.builtin.args[0];
        sparp_equiv_t *arg1_eq;
	if (SPAR_IS_BLANK_OR_VAR (arg1))
	  arg1_eq = sparp_equiv_get (sparp, curr, arg1, 0);
	else
	  break;
        switch (filt->_.builtin.btype)
          {
          case isIRI_L:
          case isURI_L:
            arg1_eq->e_rvr.rvrRestrictions |= SPART_VARR_IS_REF | SPART_VARR_IS_IRI | SPART_VARR_NOT_NULL;
            return 1;
          case isBLANK_L:
            arg1_eq->e_rvr.rvrRestrictions |= SPART_VARR_IS_REF | SPART_VARR_IS_BLANK | SPART_VARR_NOT_NULL;
            return 1;
          case isLITERAL_L:
            arg1_eq->e_rvr.rvrRestrictions |= SPART_VARR_IS_LIT | SPART_VARR_NOT_NULL;
            return 1;
          case BOUND_L:
            arg1_eq->e_rvr.rvrRestrictions |= SPART_VARR_NOT_NULL;
            return 1;
          case SAMETERM_L:
            {
              SPART *arg2 = filt->_.builtin.args[1];
              spar_var_eq_to_equiv (sparp, curr, arg1_eq, arg2); /* No return because sameTerm is more strict than merge of equivs */
              break;
            }
#ifdef DEBUG
          case IRI_L:
          case STR_L:
          case LANG_L: case LANGMATCHES_L: case DATATYPE_L:
          case REGEX_L:
          case LIKE_L:
          case IN_L:
            break;
          default: spar_internal_error (sparp, "sparp_" "filter_to_equiv(): unsupported built-in");
#else
          default: break;
#endif
        }
      break;
      }
    default: break;
    }
  return 0;
}

int
sparp_gp_trav_restrict_by_simple_filters_gp_in (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  int fctr;
  switch (SPART_TYPE(curr))
    {
    case SPAR_GP:
      break;
    case SPAR_TRIPLE: return SPAR_GPT_NODOWN;
    default: return 0;
    }
  for (fctr = BOX_ELEMENTS (curr->_.gp.filters); fctr--; /* no step */)
    { /* The descending order of fctr values is important -- note possible sparp_gp_detach_filter () */
      SPART *filt = curr->_.gp.filters[fctr];
      int ret;
      if (BOP_OR == SPART_TYPE (filt))
        {
          ret = sparp_optimize_BOP_OR_filter (sparp, curr, filt);
          if (0 == ret)
            continue;
        }
      ret = sparp_filter_to_equiv (sparp, curr, filt);
      if (0 == ret)
        continue;
      sparp_gp_detach_filter (sparp, curr, fctr, NULL);
    }
  return 0;
}

int
sparp_gp_trav_restrict_by_simple_filters_expn_subq (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  sparp_continue_gp_trav_in_sub (sparp, curr, common_env);
  return 0;
}


void
sparp_restrict_by_simple_filters (sparp_t *sparp)
{
  sparp_gp_trav (sparp, sparp->sparp_expr->_.req_top.pattern, NULL,
    sparp_gp_trav_restrict_by_simple_filters_gp_in, NULL,
    NULL, NULL, sparp_gp_trav_restrict_by_simple_filters_expn_subq,
    NULL );
}


/* Composing equivs for common names without local variables.
\c trav_env_this points to dk_set_t of names.
\c common_env is not used. */

int
sparp_gp_trav_make_common_eqs_in (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  switch (SPART_TYPE(curr))
    {
    case SPAR_GP:
      if (UNION_L == curr->_.gp.subtype)
        return 0;
      if (sts_this == sparp->sparp_stss+1)
        {
          sts_this[0].sts_env = ((dk_set_t *)common_env)[0];
          ((dk_set_t *)common_env)[0] = NULL;
        }
      else
        {
          if (NULL != ((dk_set_t *)common_env)[0])
            GPF_T;
      sts_this[0].sts_env = NULL;
        }
      return SPAR_GPT_ENV_PUSH;      
    case SPAR_TRIPLE: return SPAR_GPT_NODOWN;
    default: return 0;
    }
}

/*! For each duplicate name in \c local_vars the procedure adds new eq to \c curr.
\returns variables that can me propagated */
dk_set_t
sparp_create_eqs_for_dupe_locals (sparp_t *sparp, SPART *curr, dk_set_t *local_vars)
{
  int eq_ctr;
  caddr_t var_name;
  dk_set_t vars_to_propagate = NULL;
  while (NULL != local_vars[0])
    {
      var_name = dk_set_pop (local_vars);
      if (-1 != dk_set_position_of_string (local_vars[0], var_name))
        {
          sparp_equiv_get (sparp, curr, (SPART *)var_name, SPARP_EQUIV_INS_CLASS | SPARP_EQUIV_GET_NAMESAKES);
        }
      else
        dk_set_push (&vars_to_propagate, var_name);
    }
  SPARP_FOREACH_GP_EQUIV(sparp,curr,eq_ctr,eq)
    {
      int varname_ctr;
      DO_BOX_FAST (caddr_t, vname, varname_ctr, eq->e_varnames)
        {
          if (SPART_VARNAME_IS_GLOB (vname))
            continue;
          if (-1 == dk_set_position_of_string (vars_to_propagate, vname))
	    dk_set_push (&vars_to_propagate, vname);
        }
      END_DO_BOX_FAST;
    }
  END_SPARP_FOREACH_GP_EQUIV;
  return vars_to_propagate;
}

int
sparp_gp_trav_make_common_eqs_out (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  dk_set_t *local_vars;
  dk_set_t vars_to_propagate;
  switch (SPART_TYPE(curr))
    {
    case SPAR_GP: break;
    case SPAR_TRIPLE: return SPAR_GPT_NODOWN;
    default: return 0;
    }
  local_vars = (dk_set_t *)(&(sts_this[0].sts_env));
  vars_to_propagate = sparp_create_eqs_for_dupe_locals (sparp, curr, local_vars);
  if (sts_this == sparp->sparp_stss+1)
    {
      if (NULL != ((dk_set_t *)common_env)[0])
        GPF_T;
      ((dk_set_t *)common_env)[0] = vars_to_propagate;
    }
  else
    {
      dk_set_t *parent_vars = (dk_set_t *)(&(sts_this[-1].sts_env));
      while (NULL != vars_to_propagate)
        dk_set_push (parent_vars, dk_set_pop (&vars_to_propagate));
    }
  return 0;
}

int
sparp_gp_trav_make_common_eqs_expn_subq (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  dk_set_t *parent_vars;
  dk_set_t vars_to_propagate = NULL;
  sparp_t *sub_sparp = sparp_down_to_sub (sparp, curr);
  sparp_make_common_eqs (sub_sparp);
  sparp_up_from_sub (sparp, curr, sub_sparp);
  if (sts_this == sparp->sparp_stss+1)
    parent_vars = ((dk_set_t *)common_env);
  else
    parent_vars = (dk_set_t *)(&(sts_this[-1].sts_env));
  vars_to_propagate = sparp_create_eqs_for_dupe_locals (sparp, curr, (dk_set_t *)common_env);
  while (NULL != vars_to_propagate)
    dk_set_push (parent_vars, dk_set_pop (&vars_to_propagate));
  return 0;
}

void
sparp_make_common_eqs (sparp_t *sparp)
{
  dk_set_t top_vars_to_propagate = NULL;
  sparp_trav_out_clauses (sparp, sparp->sparp_expr, &top_vars_to_propagate,
    sparp_gp_trav_make_common_eqs_in, sparp_gp_trav_make_common_eqs_out,
    NULL, NULL, sparp_gp_trav_make_common_eqs_expn_subq,
    NULL );
  sparp_gp_trav (sparp, sparp->sparp_expr->_.req_top.pattern, &top_vars_to_propagate,
    sparp_gp_trav_make_common_eqs_in, sparp_gp_trav_make_common_eqs_out,
    NULL, NULL, sparp_gp_trav_make_common_eqs_expn_subq,
    NULL );
  while (NULL != top_vars_to_propagate)
   dk_set_pop (&top_vars_to_propagate);
}

/* Composing aliases.
\c trav_env_this points to the innermost graph pattern.
\c common_env is used in sparp_gp_trav_make_retval_aliases to pass vector of query return variables. */

void
sparp_gp_add_chain_aliases (sparp_t *sparp, SPART *inner_var, sparp_equiv_t *inner_eq, sparp_trav_state_t *sts_this, SPART *top_gp)
{
  sparp_trav_state_t *sts_iter = sts_this;
  sparp_equiv_t *curr_eq = inner_eq;
#ifdef DEBUG
  if (NULL == curr_eq)
    spar_internal_error (sparp, "sparp_" "gp_add_chain_aliases () has NULL eq for inner_var");
  if (curr_eq->e_gp != sts_iter[0].sts_env)
    spar_internal_error (sparp, "sparp_" "gp_add_chain_aliases () has eq for inner_var not equal to sts_iter[0].sts_env");
#endif
  for (;;)
    {
      sparp_equiv_t *parent_eq;
      SPART *parent_gp = sts_iter[-1].sts_env;
      if (NULL == parent_gp)
        break;
      parent_eq = sparp_equiv_get (sparp, parent_gp, inner_var, SPARP_EQUIV_GET_NAMESAKES | SPARP_EQUIV_INS_CLASS);
      sparp_equiv_connect (sparp, parent_eq, curr_eq, 1);
      if (parent_gp == top_gp)
        break;
      curr_eq = parent_eq;
      sts_iter--;
    }
}

int
sparp_gp_trav_make_retval_aliases (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  SPART **retvars = (SPART **)common_env;
  int retvar_ctr;
  switch (SPART_TYPE(curr))
    {
    case SPAR_GP: break;
    case SPAR_TRIPLE: return SPAR_GPT_NODOWN;
    default: return 0;
    }
  sts_this[0].sts_env = curr;
  for (retvar_ctr = BOX_ELEMENTS (retvars); retvar_ctr--; /* no step */)
    {
      SPART *curr_retvar = retvars[retvar_ctr];
      sparp_equiv_t *curr_eq;
      caddr_t curr_varname = spar_var_name_of_ret_column (curr_retvar);
      if (NULL == curr_varname)
        continue;
      curr_eq = sparp_equiv_get (sparp, curr, (SPART *)curr_varname, SPARP_EQUIV_GET_NAMESAKES);
      if (NULL != curr_eq)
        {
          if (SPAR_ALIAS == SPART_TYPE(curr_retvar))
            curr_retvar = curr_retvar->_.alias.arg;
        sparp_gp_add_chain_aliases (sparp, curr_retvar, curr_eq, sts_this, NULL);
    }
    }
  return SPAR_GPT_ENV_PUSH;
}

int
sparp_gp_trav_make_common_aliases_gp_in (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  int eq_ctr;
  sparp_trav_state_t *outer_gp_sts;
  switch (SPART_TYPE(curr))
    {
    case SPAR_GP: break;
    case SPAR_TRIPLE: return SPAR_GPT_NODOWN;
    default: return 0;
    }
  sts_this[0].sts_env = curr;
  SPARP_FOREACH_GP_EQUIV(sparp,curr,eq_ctr,eq)
    {
#if 0
      int var_ctr;
      for (var_ctr = eq->e_var_count; var_ctr--; /* no step */)
        {
	  SPART *var = eq->e_vars[var_ctr];
          for (outer_gp_sts = sparp->sparp_stss+1; outer_gp_sts < sts_this; outer_gp_sts++)
            {
              SPART *outer_gp = outer_gp_sts->sts_env;
	      sparp_equiv_t *topmost_eq = sparp_equiv_get (sparp, outer_gp, var, SPARP_EQUIV_GET_NAMESAKES);
	      if (NULL != topmost_eq)
		{
		  sparp_gp_add_chain_aliases (sparp, var, eq, sts_this, outer_gp);
		  break;
		}
            }
        }
#else
      int varname_ctr;
      DO_BOX_FAST (caddr_t, vname, varname_ctr, eq->e_varnames)
        {
          for (outer_gp_sts = sparp->sparp_stss+1; outer_gp_sts < sts_this; outer_gp_sts++)
            {
              SPART *outer_gp = outer_gp_sts->sts_env;
	      sparp_equiv_t *topmost_eq = sparp_equiv_get (sparp, outer_gp, (SPART *)vname, SPARP_EQUIV_GET_NAMESAKES);
	      if (NULL != topmost_eq)
		{
		  sparp_gp_add_chain_aliases (sparp, (SPART *) vname, eq, sts_this, outer_gp);
		  break;
		}
            }
        }
      END_DO_BOX_FAST;
#endif
    } END_SPARP_FOREACH_GP_EQUIV;
  return SPAR_GPT_ENV_PUSH;
}

int
sparp_gp_trav_make_aliases_expn_subq (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  sparp_t *sub_sparp = sparp_down_to_sub (sparp, curr);
  sparp_make_aliases (sub_sparp);
  sparp_up_from_sub (sparp, curr, sub_sparp);
  return 0;
}

int
sparp_gp_trav_remove_unused_aliases (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  int eq_ctr;
  switch (SPART_TYPE(curr))
    {
    case SPAR_GP: break;
    case SPAR_TRIPLE: return SPAR_GPT_NODOWN;
    default: return 0;
    }
  sts_this[0].sts_env = curr;
  SPARP_REVFOREACH_GP_EQUIV(sparp,curr,eq_ctr,eq)
    {
      int sub_ctr;
      if (
        (0 != eq->e_const_reads) ||
        (0 != eq->e_gspo_uses) ||
        (0 != eq->e_subquery_uses) ||
        (0 != BOX_ELEMENTS_INT_0 (eq->e_receiver_idxs)) ||
        (1 < (eq->e_nested_bindings + eq->e_optional_reads)) ||
        (0 != eq->e_var_count) ||
        (SPART_VARR_EXPORTED & eq->e_rvr.rvrRestrictions) )
	continue;
      for (sub_ctr = BOX_ELEMENTS_INT_0 (eq->e_subvalue_idxs); sub_ctr--; /* no step */)
        {
          sparp_equiv_t *sub_eq = SPARP_EQUIV(sparp, eq->e_subvalue_idxs[sub_ctr]);
          sparp_equiv_disconnect (sparp, eq, sub_eq);
        }
      sparp_equiv_remove (sparp, eq);
    }
  END_SPARP_REVFOREACH_GP_EQUIV;
  return SPAR_GPT_ENV_PUSH;
}


void
sparp_make_aliases (sparp_t *sparp)
{
  SPART **retvars = sparp->sparp_expr->_.req_top.retvals;
  SPART *top = sparp->sparp_expr;
  SPART *top_pattern = top->_.req_top.pattern;
  /*int retvar_ctr;*/
  sparp_gp_trav (sparp, top_pattern, retvars,
    sparp_gp_trav_make_retval_aliases, sparp_gp_trav_cu_out_triples_1,
    NULL, NULL, NULL,
    NULL );
  sparp_gp_trav (sparp, top_pattern, NULL,
    sparp_gp_trav_make_common_aliases_gp_in, sparp_gp_trav_cu_out_triples_1,
    NULL, NULL, sparp_gp_trav_make_aliases_expn_subq,
    NULL );
  sparp_gp_trav (sparp, top_pattern, NULL,
    sparp_gp_trav_remove_unused_aliases, sparp_gp_trav_cu_out_triples_1,
    NULL, NULL, NULL,
    NULL );
  sparp_trav_out_clauses (sparp, top, NULL,
    NULL, sparp_gp_trav_cu_out_triples_1,
    NULL, NULL, sparp_gp_trav_make_aliases_expn_subq,
    NULL );
}

int
sparp_gp_trav_label_external_vars_gp_in (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  int eqctr;
  dk_set_t parent_gps = (dk_set_t)common_env;
  if (SPAR_GP != SPART_TYPE (curr))
    return 0;
  if (SELECT_L == curr->_.gp.subtype)
    return SPAR_GPT_NODOWN;
  if (NULL == parent_gps)
    return SPAR_GPT_ENV_PUSH;
  SPARP_FOREACH_GP_EQUIV (sparp, curr, eqctr, eq)
    {
      int varnamectr;
      DO_BOX_FAST_REV (caddr_t, varname, varnamectr, eq->e_varnames)
        {
          DO_SET (SPART *, parent, &parent_gps)
            {
              sparp_equiv_t *parent_eq = sparp_equiv_get (sparp, parent, (SPART *)varname, SPARP_EQUIV_GET_NAMESAKES);
              if ((NULL != parent_eq) &&
                ((0 < parent_eq->e_gspo_uses) ||
                  (0 < parent_eq->e_subquery_uses) ||
                  (0 < parent_eq->e_nested_bindings) ) )
                {
/*
                  if ((SPART_BAD_EQUIV_IDX != eq->e_external_src_idx) &&
                    (parent_eq->e_own_idx != eq->e_external_src_idx) &&
                    !(parent_eq->e_rvr.rvrRestrictions & SPART_VARR_EXTERNAL) )
                    spar_internal_error (sparp, "sparp_" "gp_trav_label_external_vars_gp_in (): mismatch in origin of external");
*/
                  eq->e_external_src_idx = parent_eq->e_own_idx;
                  eq->e_rvr.rvrRestrictions |= SPART_VARR_EXTERNAL;
                  goto next_varname;
                }
            }
          END_DO_SET ()
next_varname: ;
        }
      END_DO_BOX_FAST_REV;
    }
  END_SPARP_FOREACH_GP_EQUIV;
  return SPAR_GPT_ENV_PUSH;
}

int
sparp_gp_trav_label_external_vars_expn_subq (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  dk_set_t parent_gps = (dk_set_t)common_env;
  SPART *anc_gp = sts_this->sts_ancestor_gp;
  sparp_t *sub_sparp = sparp_down_to_sub (sparp, curr);
  if (NULL != anc_gp)
    {
      s_node_t tmp_env;
      tmp_env.data = anc_gp;
      tmp_env.next = parent_gps;
      sparp_label_external_vars (sub_sparp, &tmp_env);
    }
  sparp_label_external_vars (sub_sparp, parent_gps);
  sparp_up_from_sub (sparp, curr, sub_sparp);
  return 0;
}

void
sparp_label_external_vars (sparp_t *sparp, dk_set_t parent_gps)
{
  SPART *top = sparp->sparp_expr;
  SPART *top_pattern = top->_.req_top.pattern;
  s_node_t tmp_env;
  tmp_env.data = top_pattern;
  tmp_env.next = parent_gps;
  sparp_trav_out_clauses (sparp, top, &tmp_env,
    sparp_gp_trav_label_external_vars_gp_in, NULL,
    NULL, NULL, sparp_gp_trav_label_external_vars_expn_subq,
    NULL );
  sparp_gp_trav (sparp, top_pattern, parent_gps,
    sparp_gp_trav_label_external_vars_gp_in, NULL,
    NULL, NULL, sparp_gp_trav_label_external_vars_expn_subq,
    NULL );
}

int
sparp_gp_trav_remove_redundant_connections (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  sparp_t *sub_sparp = sparp_down_to_sub (sparp, curr);
  sparp_remove_redundant_connections (sub_sparp);
  sparp_up_from_sub (sparp, curr, sub_sparp);
  return 0;
}

void
sparp_remove_redundant_connections (sparp_t *sparp)
{
  SPART *top = sparp->sparp_expr;
  SPART *top_pattern = top->_.req_top.pattern;
  sparp_equiv_t **equivs = sparp->sparp_equivs;
  int eq_ctr;
  sparp_gp_trav (sparp, top_pattern, NULL,
    sparp_gp_trav_remove_unused_aliases, sparp_gp_trav_cu_out_triples_1,
    NULL, NULL, sparp_gp_trav_remove_redundant_connections,
    NULL );
  sparp_trav_out_clauses (sparp, top, NULL,
    NULL, sparp_gp_trav_cu_out_triples_1,
    NULL, NULL, sparp_gp_trav_remove_redundant_connections,
    NULL );
  for (eq_ctr = sparp->sparp_equiv_count; eq_ctr--; /*no step*/)
    {
      sparp_equiv_t *eq = equivs[eq_ctr];
      int sub_ctr;
      if (NULL == eq)
        continue;
      if (!((SPART_VARR_GLOBAL | SPART_VARR_EXTERNAL | SPART_VARR_FIXED) & eq->e_rvr.rvrRestrictions))
        continue;
      for (sub_ctr = BOX_ELEMENTS_INT_0 (eq->e_subvalue_idxs); sub_ctr--; /*no step*/)
        {
          int can_unlink = 0;
          sparp_equiv_t *sub_eq = equivs[eq->e_subvalue_idxs[sub_ctr]];
          if (!((SPART_VARR_GLOBAL | SPART_VARR_EXTERNAL | SPART_VARR_FIXED) & sub_eq->e_rvr.rvrRestrictions))
            continue;
          if (
            ((SPART_VARR_GLOBAL | SPART_VARR_EXTERNAL) & eq->e_rvr.rvrRestrictions) &&
            ((SPART_VARR_GLOBAL | SPART_VARR_EXTERNAL) & sub_eq->e_rvr.rvrRestrictions) &&
            (1 == BOX_ELEMENTS (eq->e_varnames)) &&
            (1 == BOX_ELEMENTS (sub_eq->e_varnames)) &&
            !strcmp (eq->e_varnames[0], sub_eq->e_varnames[0]) )
            can_unlink = 1;
          if (sparp_equivs_have_same_fixedvalue (sparp, eq, sub_eq))
            can_unlink = 1;
          if (can_unlink)
            sparp_equiv_disconnect (sparp, eq, sub_eq);
        }
    }
  sparp_gp_trav (sparp, top_pattern, NULL,
    sparp_gp_trav_remove_unused_aliases, NULL,
    NULL, NULL, NULL,
    NULL );
}

/* Copying restrictions from equivalences to variables */

void
sparp_restr_of_select_eq_from_connected_subvalues (sparp_t *sparp, sparp_equiv_t *eq)
{
  int retvalctr;
  SPART *gp = eq->e_gp;
  caddr_t vname = eq->e_varnames[0];
  SPART *sub_expn;
  DO_BOX_FAST (SPART *, retval, retvalctr, gp->_.gp.subquery->_.req_top.orig_retvals)
    {
      if ((SPAR_ALIAS != SPART_TYPE (retval)) || strcmp (retval->_.alias.aname, vname))
        continue;
      sub_expn = retval->_.alias.arg;
      if (SPAR_IS_BLANK_OR_VAR(sub_expn))
        {
          sparp_equiv_t *eq_sub = sparp_equiv_get (sparp, gp->_.gp.subquery->_.req_top.pattern, sub_expn, 0);
          sparp_rvr_tighten (sparp, &(eq->e_rvr), &(eq_sub->e_rvr), ~0);
          /* !!!TBD: sparp_rvr_tighten (sparp, eq, eq_sub, 1); */
        }
      else
        {
          ptrlong restr_bits = sparp_restr_bits_of_expn (sparp, sub_expn);
          eq->e_rvr.rvrRestrictions |= restr_bits & (
            SPART_VARR_IS_REF | SPART_VARR_IS_IRI | SPART_VARR_IS_BLANK |
            SPART_VARR_IS_LIT | SPART_VARR_LONG_EQ_SQL |
            SPART_VARR_NOT_NULL | SPART_VARR_ALWAYS_NULL );
        }
      break;
    }
  END_DO_BOX_FAST;
}

void
sparp_restr_of_union_eq_from_connected_subvalues (sparp_t *sparp, sparp_equiv_t *eq)
{
  sparp_equiv_t **equivs = sparp->sparp_equivs;
  int sub_ctr, varname_ctr;
  int nice_sub_count = 0;
  dk_set_t common_vars = NULL;
  rdf_val_range_t acc;
  memset (&acc, 0, sizeof (rdf_val_range_t));
  acc.rvrRestrictions = SPART_VARR_CONFLICT;
  DO_BOX_FAST (ptrlong, sub_eq_idx, sub_ctr, eq->e_subvalue_idxs)
    {
      sparp_equiv_t *sub_eq = equivs[sub_eq_idx];
      SPART *sub_gp = sub_eq->e_gp;
          DO_BOX_FAST (caddr_t, varname, varname_ctr, sub_eq->e_varnames)
            {
              if (!SPART_VARNAME_IS_GLOB (varname))
                continue;
          if (0 > dk_set_position_of_string (common_vars, varname))
            continue;
              t_set_push (&common_vars, varname);
            }
          END_DO_BOX_FAST;
      if (sub_eq->e_rvr.rvrRestrictions & SPART_VARR_CONFLICT)
        nice_sub_count++; /* Conflict is nice because it will be removed soon */
      else
        {
          sparp_rvr_loose (sparp, &acc, &(sub_eq->e_rvr), ~0);
          if (OPTIONAL_L != sub_gp->_.gp.subtype)
            nice_sub_count++;
        }
    }
  END_DO_BOX_FAST;
  if (nice_sub_count < BOX_ELEMENTS (eq->e_gp->_.gp.members))
    acc.rvrRestrictions &= ~SPART_VARR_NOT_NULL;
  sparp_rvr_tighten (sparp, &(eq->e_rvr), &acc, ~0);
  if (NULL == common_vars)
    eq->e_rvr.rvrRestrictions &= ~(SPART_VARR_GLOBAL | SPART_VARR_EXTERNAL);
  else
    {
      DO_BOX_FAST (caddr_t, varname, varname_ctr, eq->e_varnames)
        {
          if (0 > dk_set_position_of_string (common_vars, varname))
            t_set_push (&common_vars, varname);
        }
      END_DO_BOX_FAST;
      eq->e_varnames = t_list_to_array (common_vars);
    }            
}

void
sparp_restr_of_join_eq_from_connected_subvalues (sparp_t *sparp, sparp_equiv_t *eq)
{
  sparp_equiv_t **equivs = sparp->sparp_equivs;
  int sub_ctr;
  DO_BOX_FAST (ptrlong, sub_eq_idx, sub_ctr, eq->e_subvalue_idxs)
    {
      sparp_equiv_t *sub_eq = equivs[sub_eq_idx];
      SPART *sub_gp = sub_eq->e_gp;
      if (OPTIONAL_L == sub_gp->_.gp.subtype)
        {
          if ((1 == eq->e_nested_bindings) &&
            (0 == BOX_ELEMENTS_0 (eq->e_receiver_idxs)) &&
            (0 == eq->e_gspo_uses) && (0 == eq->e_subquery_uses) )
            sparp_rvr_tighten (sparp, &(eq->e_rvr), &(sub_eq->e_rvr), ~SPART_VARR_NOT_NULL);
        }
      else
        sparp_rvr_tighten (sparp, &(eq->e_rvr), &(sub_eq->e_rvr), ~0);
    }
  END_DO_BOX_FAST;
}


int
sparp_gp_trav_eq_restr_from_connected_subvalues_gp_out (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  int eq_ctr;
  if (SPAR_GP != SPART_TYPE(curr))
    return 0;
  SPARP_FOREACH_GP_EQUIV(sparp,curr,eq_ctr,eq)
    {
      switch (curr->_.gp.subtype)
        {
        case SELECT_L: sparp_restr_of_select_eq_from_connected_subvalues (sparp, eq); break;
        case UNION_L: sparp_restr_of_union_eq_from_connected_subvalues (sparp, eq); break;
        default: sparp_restr_of_join_eq_from_connected_subvalues (sparp, eq); break;
        }
    } END_SPARP_FOREACH_GP_EQUIV;
  return 0;
}

int
sparp_gp_trav_eq_restr_from_connected_subvalues_expn_subq (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  sparp_continue_gp_trav_in_sub (sparp, curr, common_env);
  return 0;
}

int
sparp_gp_trav_eq_restr_from_connected_receivers_gp_in (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  sparp_equiv_t **equivs = sparp->sparp_equivs;
  int sub_ctr;
  int eq_ctr;
  switch (SPART_TYPE(curr))
    {
    case SPAR_GP:
      break;
    case SPAR_TRIPLE:
      return SPAR_GPT_NODOWN;
    default: return 0;
    }
  SPARP_FOREACH_GP_EQUIV(sparp,curr,eq_ctr,eq)
    {
      DO_BOX_FAST (ptrlong, sub_eq_idx, sub_ctr, eq->e_subvalue_idxs)
       {
         sparp_equiv_t *sub_eq = equivs[sub_eq_idx];
         SPART *sub_gp = sub_eq->e_gp;
         int changeable =
           ( SPART_VARR_CONFLICT |
             SPART_VARR_IS_BLANK | SPART_VARR_IS_IRI |
             SPART_VARR_IS_LIT | SPART_VARR_IS_REF |
             SPART_VARR_TYPED | SPART_VARR_FIXED |
             SPART_VARR_SPRINTFF | SPART_VARR_LONG_EQ_SQL );
         if (UNION_L != sub_gp->_.gp.subtype)
           changeable |= SPART_VARR_NOT_NULL;
         sparp_equiv_tighten (sparp, sub_eq, &(eq->e_rvr), changeable);
       }
      END_DO_BOX_FAST;
    } END_SPARP_FOREACH_GP_EQUIV;
  return 0;
}

int
sparp_gp_trav_eq_restr_from_connected_receivers_expn_subq (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  sparp_continue_gp_trav_in_sub (sparp, curr, common_env);
  return 0;
}

void
sparp_eq_restr_from_connected (sparp_t *sparp)
{
  /*sparp_env_t *env = sparp->sparp_env;*/
  sparp_gp_trav (sparp, sparp->sparp_expr->_.req_top.pattern, NULL,
    NULL, sparp_gp_trav_eq_restr_from_connected_subvalues_gp_out,
    NULL, NULL, sparp_gp_trav_eq_restr_from_connected_subvalues_expn_subq,
    NULL );
  sparp_gp_trav (sparp, sparp->sparp_expr->_.req_top.pattern, NULL,
    sparp_gp_trav_eq_restr_from_connected_receivers_gp_in, NULL,
    NULL, NULL, sparp_gp_trav_eq_restr_from_connected_receivers_expn_subq,
    NULL );
}

/* Copying restrictions from equivalences to variables */

int
sparp_gp_trav_eq_restr_to_vars_gp_in (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  int eq_ctr, var_ctr;
  switch (SPART_TYPE(curr))
    {
    case SPAR_GP:
      break;
    case SPAR_TRIPLE:
      return SPAR_GPT_NODOWN;
    default: return 0;
    }
  SPARP_FOREACH_GP_EQUIV(sparp,curr,eq_ctr,eq)
    {
      for (var_ctr = eq->e_var_count; var_ctr--; /* no step */)
	{
	  SPART *var = eq->e_vars[var_ctr];
#ifdef DEBUG
	  if ((SPAR_VARIABLE != SPART_TYPE(var)) && (SPAR_BLANK_NODE_LABEL != SPART_TYPE(var)))
	    spar_internal_error (sparp, "Not a variable in equiv in sparp_gp_trav_eq_restr_to_vars()");
#endif
	  sparp_rvr_tighten (sparp, &(var->_.var.rvr), &(eq->e_rvr), ~0);
	  var->_.var.equiv_idx = eq->e_own_idx;
	}
    } END_SPARP_FOREACH_GP_EQUIV;
  return 0;
}

int
sparp_gp_trav_eq_restr_to_vars_expn_subq (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  sparp_continue_gp_trav_in_sub (sparp, curr, common_env);
  return 0;
}

void
sparp_eq_restr_to_vars (sparp_t *sparp)
{
  /*sparp_env_t *env = sparp->sparp_env;*/
  sparp_gp_trav (sparp, sparp->sparp_expr->_.req_top.pattern, NULL,
    sparp_gp_trav_eq_restr_to_vars_gp_in, NULL,
    NULL, NULL, sparp_gp_trav_eq_restr_to_vars_expn_subq,
    NULL );
}

#ifdef DEBUG

static int
sparp_gp_trav_equiv_audit_inner_vars (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  SPART *gp = sts_this->sts_ancestor_gp;
  int eq_ctr;
  switch (SPART_TYPE(curr))
    {
    case SPAR_GP:
      SPARP_FOREACH_GP_EQUIV (sparp, curr, eq_ctr, eq)
        {
          int recv_ctr;
          DO_BOX_FAST (ptrlong, recv_idx, recv_ctr, eq->e_receiver_idxs)
            {
              sparp_equiv_t *recv = SPARP_EQUIV (sparp, recv_idx);
              if (recv->e_gp != gp)
                spar_audit_error (sparp, "sparp_" "gp_trav_equiv_audit_inner_vars(): gp of recv eq is not parent of gp of curr eq, gp %s eq %# for %s", gp->_.gp.selid, eq->e_own_idx, eq->e_varnames[0]);
            }
          END_DO_BOX_FAST;
        }
      END_SPARP_FOREACH_GP_EQUIV;
      if (NULL != curr->_.gp.subquery)
        {
          sparp_trav_state_t stss [SPARP_MAX_SYNTDEPTH+2];
          memset (stss, 0, sizeof (sparp_trav_state_t) * (SPARP_MAX_SYNTDEPTH+2));
          stss[0].sts_ofs_of_curr_in_array = -1;
          stss[1].sts_ancestor_gp = curr->_.gp.subquery->_.req_top.pattern;
          sparp_gp_trav_int (sparp, curr->_.gp.subquery->_.req_top.pattern, stss+1, common_env,
            sparp_gp_trav_equiv_audit_inner_vars, NULL,
            sparp_gp_trav_equiv_audit_inner_vars, NULL, sparp_gp_trav_equiv_audit_inner_vars,
            NULL );
          sparp_equiv_audit_retvals (sparp, curr->_.gp.subquery);
          return SPAR_GPT_NODOWN;
        }
      return SPAR_GPT_ENV_PUSH;
    case SPAR_VARIABLE: break;
    case SPAR_BLANK_NODE_LABEL: break;
    default: return 0;
    }
  if (SPART_BAD_EQUIV_IDX != curr->_.var.equiv_idx)
    {
      sparp_equiv_t *eq_by_id, *eq;
      eq_by_id = SPARP_EQUIV (sparp, curr->_.var.equiv_idx);
      if (eq_by_id->e_gp != gp)
        spar_audit_error (sparp, "sparp_" "gp_trav_equiv_audit_inner_vars(): e_gp of eq_by_id does not match gp, gp %s var %s/%s/%s", gp->_.gp.selid, curr->_.var.selid, curr->_.var.tabid, curr->_.var.vname);
      eq = sparp_equiv_get (sparp, gp, curr, 0);
      if (NULL == eq)
        {
          spar_audit_error (sparp, "sparp_" "gp_trav_equiv_audit_inner_vars(): eq not found, gp %s var %s/%s/%s", gp->_.gp.selid, curr->_.var.selid, curr->_.var.tabid, curr->_.var.vname);
          return 0;
        }
      if (eq->e_own_idx != curr->_.var.equiv_idx)
        spar_audit_error (sparp, "sparp_" "gp_trav_equiv_audit_inner_vars(): eq idx mismatch, gp %s var %s/%s/%s", gp->_.gp.selid, curr->_.var.selid, curr->_.var.tabid, curr->_.var.vname);
      if (strcmp (gp->_.gp.selid, curr->_.var.selid))
        spar_audit_error (sparp, "sparp_" "gp_trav_equiv_audit_inner_vars(): var selid differs from gp selid, gp %s var %s/%s/%s", gp->_.gp.selid, curr->_.var.selid, curr->_.var.tabid, curr->_.var.vname);
    }
  else if (SPARP_EQUIV_AUDIT_NOBAD & ((ptrlong)common_env))
    spar_audit_error (sparp, "sparp_" "gp_trav_equiv_audit_inner_vars(): var with SPART_BAD_EQUIV_IDX, var %s/%s/%s", curr->_.var.selid, curr->_.var.tabid, curr->_.var.vname);
  return 0;
}

static int
sparp_gp_trav_equiv_audit_retvals (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  SPART *top_gp = (SPART *)(common_env);
  sparp_equiv_t *eq;
  switch (SPART_TYPE(curr))
    {
    case SPAR_VARIABLE: break;
    case SPAR_BLANK_NODE_LABEL: break;
    default: return 0;
    }
  eq = sparp_equiv_get (sparp, top_gp, curr, SPARP_EQUIV_GET_ASSERT | SPARP_EQUIV_GET_NAMESAKES);
  if (eq->e_own_idx != curr->_.var.equiv_idx)
    spar_audit_error (sparp, "sparp_" "gp_trav_equiv_audit_retvals(): eq idx mismatch");
  if (!(eq->e_rvr.rvrRestrictions & SPART_VARR_EXPORTED))
    spar_audit_error (sparp, "sparp_" "gp_trav_equiv_audit_retvals(): lost SPART_VARR_EXPORTED");
  return 0;
}

void
sparp_equiv_audit_retvals (sparp_t *sparp, SPART *top)
{
  int ctr;
  sparp_trav_state_t stss [SPARP_MAX_SYNTDEPTH+2];
  DO_BOX_FAST (SPART *, expn, ctr, top->_.req_top.retvals)
    {
      memset (stss, 0, sizeof (sparp_trav_state_t) * (SPARP_MAX_SYNTDEPTH+2));
      stss[0].sts_ofs_of_curr_in_array = -1;
      sparp_gp_trav_int (sparp, expn, stss+1, top->_.req_top.pattern,
        NULL, NULL,
        sparp_gp_trav_equiv_audit_retvals, NULL, NULL,
        NULL );
    }
  END_DO_BOX_FAST;
  DO_BOX_FAST (SPART *, grouping, ctr, top->_.req_top.groupings)
    {
      memset (stss, 0, sizeof (sparp_trav_state_t) * (SPARP_MAX_SYNTDEPTH+2));
      stss[0].sts_ofs_of_curr_in_array = -1;
      sparp_gp_trav_int (sparp, grouping, stss+1, top->_.req_top.pattern,
        NULL, NULL,
        sparp_gp_trav_equiv_audit_retvals, NULL, NULL,
        NULL );
    }
  END_DO_BOX_FAST;
  DO_BOX_FAST (SPART *, oby, ctr, top->_.req_top.order)
    {
      memset (stss, 0, sizeof (sparp_trav_state_t) * (SPARP_MAX_SYNTDEPTH+2));
      stss[0].sts_ofs_of_curr_in_array = -1;
      sparp_gp_trav_int (sparp, oby->_.oby.expn, stss+1, top->_.req_top.pattern,
        NULL, NULL,
        sparp_gp_trav_equiv_audit_retvals, NULL, NULL,
        NULL );
    }
  END_DO_BOX_FAST;
}

void
sparp_equiv_audit_gp (sparp_t *sparp, SPART *gp, int is_deprecated, sparp_equiv_t *chk_eq)
{
  int gp_eq_ctr;
  SPARP_FOREACH_GP_EQUIV (sparp, gp, gp_eq_ctr, gp_eq)
    {
      if (gp_eq->e_gp != gp)
        spar_audit_error (sparp, "sparp_" "equiv_audit_gp(): gp_eq->e_gp != gp, gp %s eq #%d for %s", gp->_.gp.selid, gp_eq->e_own_idx, gp_eq->e_varnames[0]);
      if (chk_eq == gp_eq)
        chk_eq = NULL;
      if (gp_eq->e_deprecated && !is_deprecated)
        spar_audit_error (sparp, "sparp_" "equiv_audit_gp(): eq is deprecated, gp %s eq #%d for %s", gp->_.gp.selid, gp_eq->e_own_idx, gp_eq->e_varnames[0]);
      if (is_deprecated && !(gp_eq->e_deprecated))
        spar_audit_error (sparp, "sparp_" "equiv_audit_gp(): eq is expected to be deprecated, gp %s eq #%d for %s", gp->_.gp.selid, gp_eq->e_own_idx, gp_eq->e_varnames[0]);
    }
  END_SPARP_FOREACH_GP_EQUIV;
  if (NULL != chk_eq)
    spar_audit_error (sparp, "sparp_" "equiv_audit_gp(): no reference to chk_eq in gp, gp %s chk_eq #%d for %s", gp->_.gp.selid, chk_eq->e_own_idx, chk_eq->e_varnames[0]);
}

void
sparp_equiv_audit_all (sparp_t *sparp, int flags)
{
  sparp_trav_state_t stss [SPARP_MAX_SYNTDEPTH+2];
  int eq_ctr, var_ctr, recv_ctr, subv_ctr;
  memset (stss, 0, sizeof (sparp_trav_state_t) * (SPARP_MAX_SYNTDEPTH+2));
  stss[0].sts_ofs_of_curr_in_array = -1;
  sparp_gp_trav_int (sparp, sparp->sparp_expr->_.req_top.pattern, stss+1, (void *)((ptrlong)flags),
    sparp_gp_trav_equiv_audit_inner_vars, NULL,
    sparp_gp_trav_equiv_audit_inner_vars, NULL, sparp_gp_trav_equiv_audit_inner_vars,
    NULL );
  sparp_equiv_audit_retvals (sparp, sparp->sparp_expr);
  for (eq_ctr = sparp->sparp_equiv_count; eq_ctr--; /*no step*/)
    {
      sparp_equiv_t *eq = SPARP_EQUIV (sparp, eq_ctr);
      SPART *gp;
      if (NULL == eq)
        continue;
      if (eq->e_own_idx != eq_ctr)
        spar_audit_error (sparp, "sparp_" "equiv_audot_all(): wrong own index, eq #%d for %s has e_own_idx %d", eq_ctr, eq->e_varnames[0], eq->e_own_idx);
      if (eq->e_nested_bindings != BOX_ELEMENTS_0 (eq->e_subvalue_idxs))
        printf ("sparp_" "equiv_audit_all(): warning: strange: equiv %d (?%s): e_nested_bindings = %d, %d subvalues in list, e_optional_reads = %d\n",
            (int)(eq->e_own_idx), eq->e_varnames[0],
            (int)(eq->e_nested_bindings), BOX_ELEMENTS_INT_0 (eq->e_subvalue_idxs), (int)(eq->e_optional_reads) );
      gp = eq->e_gp;
      sparp_equiv_audit_gp (sparp, gp, ((SPART_BAD_GP_SUBTYPE == gp->_.gp.subtype) ? 1 : 0), eq);
      for (var_ctr = eq->e_var_count; var_ctr--; /*no step*/)
        {
          SPART *var = eq->e_vars [var_ctr];
          if (var->_.var.equiv_idx != eq_ctr)
            spar_audit_error (sparp, "sparp_" "equiv_audit_all(): var->_.var.equiv_idx != eq_ctr: eq #%d for %s, gp %s, var %s/%s/%s with equiv_idx %d", eq_ctr, eq->e_varnames[0], var->_.var.selid, var->_.var.tabid, var->_.var.vname, var->_.var.equiv_idx);
          if (strcmp (var->_.var.selid, gp->_.gp.selid))
            spar_audit_error (sparp, "sparp_" "equiv_audit_all(): selid of var of eq differs from selid of gp of eq, gp %s, var %s/%s/%s with equiv_idx %d", gp->_.gp.selid, var->_.var.selid, var->_.var.tabid, var->_.var.vname, var->_.var.equiv_idx);
          if (NULL != var->_.var.tabid)
            {
              int var_tr_idx = var->_.var.tr_idx;
              int triple_idx;
              for (triple_idx = BOX_ELEMENTS (gp->_.gp.members); triple_idx--; /* no step */)
                {
                  SPART *var_triple = gp->_.gp.members[triple_idx];
                  if (SPAR_TRIPLE != var_triple->type)
                    continue;
                  if (var_triple->_.triple.tr_fields[var_tr_idx] == var)
                    break;
                }
              if (0 > triple_idx)
                spar_audit_error (sparp, "sparp_" "equiv_audit_all(): var is in equiv but not in any triple of the group, var %s/%s/%s", var->_.var.selid, var->_.var.tabid, var->_.var.vname);
            }
        }
      recv_ctr = BOX_ELEMENTS_0 (eq->e_receiver_idxs);
      if (0 != recv_ctr)
        {
          SPART *recv_gp_0 = (SPARP_EQUIV (sparp, eq->e_receiver_idxs[0]))->e_gp;
          while (recv_ctr-- > 1)
            {
              SPART *recv_gp_N = (SPARP_EQUIV (sparp, eq->e_receiver_idxs[recv_ctr]))->e_gp;
              if (recv_gp_N != recv_gp_0)
                spar_audit_error (sparp, "sparp_" "equiv_audit_all(): gps of different recvs differ");
            }
          DO_BOX_FAST_REV (ptrlong, recv_idx, recv_ctr, eq->e_receiver_idxs)
            {
              sparp_equiv_t *recv = SPARP_EQUIV (sparp, recv_idx);
              DO_BOX_FAST_REV (ptrlong, subv_idx, subv_ctr, recv->e_subvalue_idxs)
                {
                  if (subv_idx == eq_ctr)
                    goto subv_of_recv_ok;
                }
              END_DO_BOX_FAST_REV;
              spar_audit_error (sparp, "sparp_" "equiv_audit_all(): no matching subv for recv");
subv_of_recv_ok: ;
            }
          END_DO_BOX_FAST_REV;
          DO_BOX_FAST_REV (ptrlong, subv_idx, subv_ctr, eq->e_subvalue_idxs)
            {
              sparp_equiv_t *subv = SPARP_EQUIV (sparp, subv_idx);
              DO_BOX_FAST_REV (ptrlong, recv_idx, recv_ctr, subv->e_receiver_idxs)
                {
                  if (recv_idx == eq_ctr)
                    goto recv_of_subv_ok;
                }
              END_DO_BOX_FAST_REV;
              spar_audit_error (sparp, "sparp_" "equiv_audit_all(): no matching recv for subv");
recv_of_subv_ok: ;
            }
          END_DO_BOX_FAST_REV;
        }
#if 0
      if ((0 == eq->e_gspo_uses) && (0 == eq->e_subquery_uses) &&
        (0 == eq->e_nested_bindings) &&
        !(eq->e_rvr.rvrRestrictions & (SPART_VARR_FIXED | SPART_VARR_GLOBAL | SPART_VARR_EXTERNAL)) &&
        ((0 != eq->e_const_reads) || /* no check for (0 != eq->e_optional_reads) */
          (0 != BOX_ELEMENTS_0 (eq->e_receiver_idxs)) /*||
          (eq->e_rvr.rvrRestrictions & SPART_VARR_EXPORTED)*/ ) )
      spar_error (sparp, "Variable '%.100s' is used but not assigned", eq->e_varnames[0]);
#endif
    }
}

void
sparp_audit_mem (sparp_t *sparp)
{
  t_check_tree (sparp->sparp_expr);
}

#endif

int sparp_gp_trav_check_if_local (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  if (SPAR_VARIABLE == curr->type)
    {
      caddr_t varname = curr->_.var.vname;
      if (SPART_VARNAME_IS_GLOB(varname))
        {
          common_env = (void *)((ptrlong)1);
          return SPAR_GPT_COMPLETED;
    }
      return SPAR_GPT_NODOWN;
    }
  return 0;
}  

int
sparp_tree_is_global_expn (sparp_t *sparp, SPART *tree)
{
  ptrlong has_locals = 0;
  sparp_gp_trav (sparp, tree, &has_locals,
    NULL, NULL,
    sparp_gp_trav_check_if_local, NULL, NULL,
    NULL );
  return !has_locals;
}

/* Main rewriting functions */

void
sparp_rewrite_basic (sparp_t *sparp)
{
  sparp_audit_mem (sparp);
  sparp_count_usages (sparp, NULL);
  sparp_audit_mem (sparp);
  sparp_restrict_by_simple_filters (sparp);
  sparp_audit_mem (sparp);
  sparp_make_common_eqs (sparp);
  sparp_make_aliases (sparp);
  sparp_eq_restr_from_connected (sparp);
  sparp_eq_restr_to_vars (sparp);
  sparp_label_external_vars (sparp, NULL);
  sparp_remove_redundant_connections (sparp);
  sparp_audit_mem (sparp);
}

/* PART 2. GRAPH PATTERN TERM REWRITING */

int
sparp_check_field_mapping_of_cvalue (sparp_t *sparp, SPART *cvalue, rdf_val_range_t *qmv_or_fmt_rvr, rdf_val_range_t *rvr)
{
  if ((NULL != rvr) && (NULL != rvr->rvrFixedValue))
    {
      if (!sparp_fixedvalues_equal (sparp, cvalue, (SPART *)(rvr->rvrFixedValue)))
        return SSG_QM_NO_MATCH;
      return SSG_QM_PROVEN_MATCH;
    }
  if (NULL != qmv_or_fmt_rvr)
            {
      if (SPART_VARR_SPRINTFF & qmv_or_fmt_rvr->rvrRestrictions)
    {
          if (!rvr_sprintffs_like ((caddr_t) cvalue, qmv_or_fmt_rvr))
            return SSG_QM_NO_MATCH;
    }
      if (SPART_VARR_FIXED & qmv_or_fmt_rvr->rvrRestrictions)
    {
          if (!sparp_fixedvalues_equal (sparp, cvalue, (SPART *)(qmv_or_fmt_rvr->rvrFixedValue)))
            return SSG_QM_NO_MATCH;
          return SSG_QM_PROVEN_MATCH;
    }
    }
  return SSG_QM_APPROX_MATCH;
}

int
sparp_check_mapping_of_sources (sparp_t *sparp, tc_context_t *tcc,
  rdf_val_range_t *qmv_or_fmt_rvr, rdf_val_range_t *rvr, int invalidation_level )
{
  int min_match = 0xFFFF;
  int source_ctr;
#ifdef DEBUG
  if (!(tcc->tcc_check_source_graphs))
    GPF_T1("Bad invocation of sparp_check_mapping_of_sources()");
#endif
  DO_BOX_FAST (SPART *, source, source_ctr, tcc->tcc_sources)
            {
      int chk_res;
      if (tcc->tcc_source_invalidation_masks[source_ctr])
        continue;
      chk_res = sparp_check_field_mapping_of_cvalue (sparp, (SPART *)(source->_.alias.arg->_.lit.val), qmv_or_fmt_rvr, rvr);
      if (SSG_QM_NO_MATCH != chk_res)
        {
          if (chk_res < min_match)
            min_match = chk_res;
        }
      else
        tcc->tcc_source_invalidation_masks[source_ctr] |= (1 << invalidation_level);
    }
  END_DO_BOX_FAST;
  if (0xFFFF == min_match)
    return SSG_QM_NO_MATCH;
  return min_match;
}

ptrdiff_t qm_field_map_offsets[4] = {
  JSO_FIELD_OFFSET(quad_map_t,qmGraphMap),
  JSO_FIELD_OFFSET(quad_map_t,qmSubjectMap),
  JSO_FIELD_OFFSET(quad_map_t,qmPredicateMap),
  JSO_FIELD_OFFSET(quad_map_t,qmObjectMap) };

ptrdiff_t qm_field_constants_offsets[4] = {
  JSO_FIELD_OFFSET(quad_map_t,qmGraphRange.rvrFixedValue),
  JSO_FIELD_OFFSET(quad_map_t,qmSubjectRange.rvrFixedValue),
  JSO_FIELD_OFFSET(quad_map_t,qmPredicateRange.rvrFixedValue),
  JSO_FIELD_OFFSET(quad_map_t,qmObjectRange.rvrFixedValue) };

int
sparp_check_field_mapping_g (sparp_t *sparp, tc_context_t *tcc, SPART *field,
  quad_map_t *qm, qm_value_t *qmv, rdf_val_range_t *rvr, int invalidation_level )
{
  int source_ctr;
  rdf_val_range_t *qmv_or_fmt_rvr = NULL;
  ptrlong field_type = SPART_TYPE (field);
  if (NULL != qmv)
    {
      if ((SPART_VARR_SPRINTFF | SPART_VARR_FIXED) & qmv->qmvRange.rvrRestrictions)
        qmv_or_fmt_rvr = &(qmv->qmvRange);
      else if (NULL != qmv->qmvFormat)
        {
          if ((SPART_VARR_SPRINTFF | SPART_VARR_FIXED) & qmv->qmvFormat->qmfValRange.rvrRestrictions)
            qmv_or_fmt_rvr = &(qmv->qmvFormat->qmfValRange);
        }
    }
  for (source_ctr = BOX_ELEMENTS_0 (tcc->tcc_sources); source_ctr--; /* no step */)
    tcc->tcc_source_invalidation_masks[source_ctr] &= (1 << invalidation_level) - 1;
  if (tcc->tcc_check_source_graphs)
        {
      int chk_res = sparp_check_mapping_of_sources (sparp, tcc, qmv_or_fmt_rvr, rvr, invalidation_level);
      if (SSG_QM_NO_MATCH == chk_res)
        return SSG_QM_NO_MATCH;
    }
  if (!tcc->tcc_check_source_graphs && (qm != tcc->tcc_qs->qsDefaultMap) &&
    (tcc->tcc_qs->qsMatchingFlags & SPART_QS_NO_IMPLICIT_USER_QM) &&
    (SPAR_BLANK_NODE_LABEL == SPART_TYPE (field)) )
    return SSG_QM_NO_MATCH;
  if ((SPAR_BLANK_NODE_LABEL == field_type) || (SPAR_VARIABLE == field_type))
    {
      if (tcc->tcc_check_source_graphs)
        {
          int chk_res = sparp_check_mapping_of_sources (sparp, tcc, &(field->_.var.rvr), NULL, invalidation_level);
          if (SSG_QM_NO_MATCH == chk_res)
            return SSG_QM_NO_MATCH;
    }
      if (SPART_VARR_FIXED & field->_.var.rvr.rvrRestrictions)
    {
          sparp_equiv_t *eq_g = sparp->sparp_equivs[field->_.var.equiv_idx];
          int chk_res;
          if (DV_UNAME != DV_TYPE_OF (eq_g->e_rvr.rvrFixedValue))
            { /* This would be very strange failure */
#ifdef DEBUG
              GPF_T1 ("sparp_check_field_mapping_g(): non-UNAME fixed value of variable used as graph of a triple. Legal but strange");
#else
              return SSG_QM_NO_MATCH;
#endif
    }
          chk_res = sparp_check_field_mapping_of_cvalue (sparp, (SPART *)(eq_g->e_rvr.rvrFixedValue), qmv_or_fmt_rvr, rvr);
          return chk_res;
        }
      if ((NULL != qmv_or_fmt_rvr) && (SPART_VARR_SPRINTFF & qmv_or_fmt_rvr->rvrRestrictions))
    {
          if (SPART_VARR_FIXED & field->_.var.rvr.rvrRestrictions)
            { /* Check if a fixed value of a field variable matches to one of sffs of the mapping value */
              caddr_t fv = rvr_string_fixedvalue (&(field->_.var.rvr));
              if (NULL != fv)
        {
                  int ctr;
                  for (ctr = qmv_or_fmt_rvr->rvrSprintffCount; ctr--; /* no step */)
                    if (sprintff_like (fv, qmv_or_fmt_rvr->rvrSprintffs[ctr]))
                      goto fixed_field_matches_qmv_sff; /* see below */
                  return SSG_QM_NO_MATCH; /* reached if no match found */

fixed_field_matches_qmv_sff: ;
	}
    }
          if (SPART_VARR_SPRINTFF & field->_.var.rvr.rvrRestrictions)
            { /* Check if either of formats of a field variable matches to one of sffs of the mapping value */
              int fld_ctr, qmv_ctr;
/* First pass is optimistic and tries to find an exact equality */
              for (fld_ctr = field->_.var.rvr.rvrSprintffCount; fld_ctr--; /* no step */)
                for (qmv_ctr = qmv_or_fmt_rvr->rvrSprintffCount; qmv_ctr--; /* no step */)
                   if (!strcmp (
                          field->_.var.rvr.rvrSprintffs [fld_ctr],
                          qmv_or_fmt_rvr->rvrSprintffs [qmv_ctr] ) )
                     goto field_sff_isects_qmv_sff;
/* Second pass checks everything, slowly */
              for (fld_ctr = field->_.var.rvr.rvrSprintffCount; fld_ctr--; /* no step */)
                for (qmv_ctr = qmv_or_fmt_rvr->rvrSprintffCount; qmv_ctr--; /* no step */)
                   if (NULL != sprintff_intersect (
                          field->_.var.rvr.rvrSprintffs [fld_ctr],
                          qmv_or_fmt_rvr->rvrSprintffs [qmv_ctr], 1 ) )
                     goto field_sff_isects_qmv_sff;
              return SSG_QM_NO_MATCH; /* reached if no match found */
field_sff_isects_qmv_sff: ;
	}
    }
      if (NULL == rvr->rvrFixedValue)
        return SSG_QM_APPROX_MATCH; /* This is not quite true, but this is the matching rule for EXCLUSIVE */
      return SSG_QM_PARTIAL_MATCH;
    }
  else if ((SPAR_LIT == field_type) || (SPAR_QNAME == field_type))
    {
      int chk_res;
      caddr_t eff_val = SPAR_LIT_OR_QNAME_VAL (field);
      if (DV_UNAME != DV_TYPE_OF (eff_val))
        { /* This would be very-very strange failure */
#ifdef DEBUG
          GPF_T1 ("sparp_check_field_mapping_g(): non-UNAME constant used as graph of a triple, legal but strange");
#else
          return SSG_QM_NO_MATCH;
#endif
    }
      if (tcc->tcc_check_source_graphs)
        {
          rdf_val_range_t fake_rvr;
          fake_rvr.rvrRestrictions = SPART_VARR_FIXED;
          fake_rvr.rvrFixedValue = eff_val;
          chk_res = sparp_check_mapping_of_sources (sparp, tcc, NULL, &fake_rvr, invalidation_level);
          if (SSG_QM_NO_MATCH == chk_res)
            return SSG_QM_NO_MATCH;
    }
      chk_res = sparp_check_field_mapping_of_cvalue (sparp, (SPART *) eff_val, qmv_or_fmt_rvr, rvr);
      return chk_res;
    }
  GPF_T1("ssg_check_field_mapping_g(): field is neither variable nor literal?");
  return SSG_QM_NO_MATCH;
}

int
sparp_check_field_mapping_spo (sparp_t *sparp, tc_context_t *tcc, SPART *field,
  quad_map_t *qm, qm_value_t *qmv, rdf_val_range_t *rvr)
{
  rdf_val_range_t *qmv_or_fmt_rvr = NULL;
  ptrlong field_type = SPART_TYPE (field);
  if (NULL != qmv)
    {
      if ((SPART_VARR_SPRINTFF | SPART_VARR_FIXED) & qmv->qmvRange.rvrRestrictions)
        qmv_or_fmt_rvr = &(qmv->qmvRange);
      else if (NULL != qmv->qmvFormat)
        {
          if ((SPART_VARR_SPRINTFF | SPART_VARR_FIXED | SPART_VARR_IS_LIT | SPART_VARR_IS_REF) & qmv->qmvFormat->qmfValRange.rvrRestrictions)
            qmv_or_fmt_rvr = &(qmv->qmvFormat->qmfValRange);
    }
    }
  if ((SPAR_BLANK_NODE_LABEL == field_type) || (SPAR_VARIABLE == field_type))
    {
      if ((NULL != rvr->rvrFixedValue) && (SPART_VARR_FIXED & field->_.var.rvr.rvrRestrictions))
        { /* Check if a fixed value of a field variable is equal to the constant field of the mapping */
          if (!sparp_fixedvalues_equal (sparp, (SPART *)(field->_.var.rvr.rvrFixedValue), (SPART *)(rvr->rvrFixedValue)))
            return SSG_QM_NO_MATCH;
          return SSG_QM_PROVEN_MATCH;
    }
      if ((NULL != qmv_or_fmt_rvr) && (SPART_VARR_SPRINTFF & qmv_or_fmt_rvr->rvrRestrictions))
    {
          if (SPART_VARR_FIXED & field->_.var.rvr.rvrRestrictions)
            { /* Check if a fixed value of a field variable matches to one of sffs of the mapping value */
              caddr_t fv = rvr_string_fixedvalue (&(field->_.var.rvr));
              if (NULL != fv)
    {
                  int ctr;
                  for (ctr = qmv_or_fmt_rvr->rvrSprintffCount; ctr--; /* no step */)
                    if (sprintff_like (fv, qmv_or_fmt_rvr->rvrSprintffs[ctr]))
                      goto fixed_field_matches_qmv_sff; /* see below */
                  return SSG_QM_NO_MATCH; /* reached if no match found */
fixed_field_matches_qmv_sff: ;
    }
    }
          if (SPART_VARR_SPRINTFF & field->_.var.rvr.rvrRestrictions)
            { /* Check if either of formats of a field variable matches to one of sffs of the mapping value */
              int fld_ctr, qmv_ctr;
/* First pass is optimistic and tries to find an exact equality */
              for (fld_ctr = field->_.var.rvr.rvrSprintffCount; fld_ctr--; /* no step */)
                for (qmv_ctr = qmv_or_fmt_rvr->rvrSprintffCount; qmv_ctr--; /* no step */)
                   if (!strcmp (
                          field->_.var.rvr.rvrSprintffs [fld_ctr],
                          qmv_or_fmt_rvr->rvrSprintffs [qmv_ctr] ) )
                     goto field_sff_isects_qmv_sff;
/* Second pass checks everything, slowly */
              for (fld_ctr = field->_.var.rvr.rvrSprintffCount; fld_ctr--; /* no step */)
                for (qmv_ctr = qmv_or_fmt_rvr->rvrSprintffCount; qmv_ctr--; /* no step */)
                   if (NULL != sprintff_intersect (
                          field->_.var.rvr.rvrSprintffs [fld_ctr],
                          qmv_or_fmt_rvr->rvrSprintffs [qmv_ctr], 1 ) )
                     goto field_sff_isects_qmv_sff;
              return SSG_QM_NO_MATCH; /* reached if no match found */
field_sff_isects_qmv_sff: ;
    }
    }
      if (NULL == rvr->rvrFixedValue)
        return SSG_QM_APPROX_MATCH; /* This is not quite true, but this is the matching rule for EXCLUSIVE */
      return SSG_QM_PARTIAL_MATCH;
    }
  else if ((SPAR_LIT == field_type) || (SPAR_QNAME == field_type))
    {
      rdf_val_range_t *some_map_rvr = ((NULL != qmv_or_fmt_rvr) ? qmv_or_fmt_rvr : rvr);
      if (SPAR_LIT == field_type)
    {
          if (SPART_VARR_IS_REF & some_map_rvr->rvrRestrictions)
            return SSG_QM_NO_MATCH;
    }
      else
                {
          if (SPART_VARR_IS_LIT & some_map_rvr->rvrRestrictions)
            return SSG_QM_NO_MATCH;
        }
      if (NULL != rvr->rvrFixedValue)
        {
          if (!sparp_fixedvalues_equal (sparp, field, (SPART *)(rvr->rvrFixedValue)))
            return SSG_QM_NO_MATCH;
          return SSG_QM_PROVEN_MATCH;
        }
      if ((NULL != qmv_or_fmt_rvr) && (SPART_VARR_SPRINTFF & qmv_or_fmt_rvr->rvrRestrictions))
            {
          caddr_t fv = SPAR_LIT_OR_QNAME_VAL (field);
          if ((NULL != fv) && IS_STRING_DTP (DV_TYPE_OF (fv)))
                {
              int ctr;
              for (ctr = qmv_or_fmt_rvr->rvrSprintffCount; ctr--; /* no step */)
                if (sprintff_like (fv, qmv_or_fmt_rvr->rvrSprintffs[ctr]))
                  return SSG_QM_PROVEN_MATCH;
              return SSG_QM_NO_MATCH; /* reached if no match found */
            }
        }
      return SSG_QM_PROVEN_MATCH;
    }
  GPF_T1("ssg_check_field_mapping_spo(): field is neither variable nor literal?");
  return SSG_QM_NO_MATCH;
}

int
sparp_check_triple_case (sparp_t *sparp, tc_context_t *tcc, quad_map_t *qm, int invalidation_level)
{
  SPART *src_g = tcc->tcc_triple->_.triple.tr_graph;
  SPART *src_s = tcc->tcc_triple->_.triple.tr_subject;
  SPART *src_p = tcc->tcc_triple->_.triple.tr_predicate;
  SPART *src_o = tcc->tcc_triple->_.triple.tr_object;
  int g_match = SSG_QM_UNSET, s_match = SSG_QM_UNSET,
    p_match = SSG_QM_UNSET, o_match = SSG_QM_UNSET;
  if (NULL == qm)
    return SSG_QM_NO_MATCH;
  /* First of all we perform check for cached matches, esp. for failures. */
  if ((NULL != qm->qmGraphMap) && (tcc->tcc_last_qmvs [SPART_TRIPLE_GRAPH_IDX] == qm->qmGraphMap))
        {
      g_match = tcc->tcc_last_qmv_results [SPART_TRIPLE_GRAPH_IDX];
      if (SSG_QM_NO_MATCH == g_match)
        return SSG_QM_NO_MATCH;
    }
  if ((NULL != qm->qmSubjectMap) && (tcc->tcc_last_qmvs [SPART_TRIPLE_SUBJECT_IDX] == qm->qmSubjectMap))
    {
      s_match = tcc->tcc_last_qmv_results [SPART_TRIPLE_SUBJECT_IDX];
      if (SSG_QM_NO_MATCH == s_match)
        return SSG_QM_NO_MATCH;
    }
  if ((NULL != qm->qmPredicateMap) && (tcc->tcc_last_qmvs [SPART_TRIPLE_PREDICATE_IDX] == qm->qmPredicateMap))
        {
      p_match = tcc->tcc_last_qmv_results [SPART_TRIPLE_PREDICATE_IDX];
      if (SSG_QM_NO_MATCH == p_match)
        return SSG_QM_NO_MATCH;
        }
  if ((NULL != qm->qmObjectMap) && (tcc->tcc_last_qmvs [SPART_TRIPLE_OBJECT_IDX] == qm->qmObjectMap))
        {
      o_match = tcc->tcc_last_qmv_results [SPART_TRIPLE_OBJECT_IDX];
      if (SSG_QM_NO_MATCH == o_match)
        return SSG_QM_NO_MATCH;
    }
  /* Check of predicate first, because it's more selective than other */
  if (SSG_QM_UNSET == p_match)
            {
      p_match = sparp_check_field_mapping_spo (sparp, tcc, src_p, qm, qm->qmPredicateMap, &(qm->qmPredicateRange));
      if (NULL != qm->qmPredicateMap)
                {
          tcc->tcc_last_qmvs [SPART_TRIPLE_PREDICATE_IDX] = qm->qmPredicateMap;
          tcc->tcc_last_qmv_results [SPART_TRIPLE_PREDICATE_IDX] = p_match;
            }
      if (SSG_QM_NO_MATCH == p_match)
        return SSG_QM_NO_MATCH;
        }
  /* Check of graph */
  if (SSG_QM_UNSET == g_match)
                    {
      g_match = sparp_check_field_mapping_g (sparp, tcc, src_g, qm, qm->qmGraphMap, &(qm->qmGraphRange), invalidation_level);
      if (NULL != qm->qmGraphMap)
                    {
          tcc->tcc_last_qmvs [SPART_TRIPLE_GRAPH_IDX] = qm->qmGraphMap;
          tcc->tcc_last_qmv_results [SPART_TRIPLE_GRAPH_IDX] = g_match;
        }
      if (SSG_QM_NO_MATCH == g_match)
        return SSG_QM_NO_MATCH;
    }
  /* Check of subject */
  if (SSG_QM_UNSET == s_match)
    {
      s_match = sparp_check_field_mapping_spo (sparp, tcc, src_s, qm, qm->qmSubjectMap, &(qm->qmSubjectRange));
      if (NULL != qm->qmSubjectMap)
    {
          tcc->tcc_last_qmvs [SPART_TRIPLE_SUBJECT_IDX] = qm->qmSubjectMap;
          tcc->tcc_last_qmv_results [SPART_TRIPLE_SUBJECT_IDX] = s_match;
    }
      if (SSG_QM_NO_MATCH == s_match)
        return SSG_QM_NO_MATCH;
        }
  /* Check of object */
  if (SSG_QM_UNSET == o_match)
        {
      o_match = sparp_check_field_mapping_spo (sparp, tcc, src_o, qm, qm->qmObjectMap, &(qm->qmObjectRange));
      if (NULL != qm->qmObjectMap)
        {
          tcc->tcc_last_qmvs [SPART_TRIPLE_OBJECT_IDX] = qm->qmObjectMap;
          tcc->tcc_last_qmv_results [SPART_TRIPLE_OBJECT_IDX] = o_match;
        }
      if (SSG_QM_NO_MATCH == o_match)
        return SSG_QM_NO_MATCH;
    }
  /* The conclusion */
  if ((SSG_QM_PROVEN_MATCH == g_match) && (SSG_QM_PROVEN_MATCH == s_match) &&
    (SSG_QM_PROVEN_MATCH == p_match) && (SSG_QM_PROVEN_MATCH == o_match) )
    return SSG_QM_PROVEN_MATCH;
  if ((SSG_QM_APPROX_MATCH <= g_match) && (SSG_QM_APPROX_MATCH <= s_match) &&
    (SSG_QM_APPROX_MATCH <= p_match) && (SSG_QM_APPROX_MATCH <= o_match) )
    return SSG_QM_APPROX_MATCH;
  return SSG_QM_PARTIAL_MATCH;
}

int
sparp_qm_find_triple_cases (sparp_t *sparp, tc_context_t *tcc, quad_map_t *qm, int inside_allowed_qm, int invalidation_level)
{
  int ctr, fld_ctr, single_fixed_fld = -1;
  int qm_is_a_good_case = 0;
  int common_status = sparp_check_triple_case (sparp, tcc, qm, invalidation_level);
  if (SSG_QM_NO_MATCH == common_status)
    return SSG_QM_NO_MATCH;
  if ((NULL == tcc->tcc_top_allowed_qm) || (qm == tcc->tcc_top_allowed_qm))
    inside_allowed_qm = 1;
  DO_BOX_FAST (quad_map_t *, sub_qm, ctr, qm->qmUserSubMaps)
        {
      int status = sparp_qm_find_triple_cases (sparp, tcc, sub_qm, inside_allowed_qm, invalidation_level+1);
      if (SSG_QM_MATCH_AND_CUT == status)
        return SSG_QM_MATCH_AND_CUT;
    }
  END_DO_BOX_FAST;
  for (;;)
    {
      if (SPART_QM_EMPTY & qm->qmMatchingFlags)
        break; /* not a good case */
      if (0 != tcc->tcc_triple->_.triple.ft_type)
        {
          qm_ftext_t *qmft;
          if (NULL == qm->qmObjectMap)
            break; /* not a good case for ft */
          qmft = qm->qmObjectMap->qmvFText;
          if (NULL == qmft)
            break; /* not a good case for ft */
        }
      qm_is_a_good_case = 1;
      break;
    }
  if (qm_is_a_good_case)
    {
      triple_case_t *tc = (triple_case_t *)t_alloc (sizeof (triple_case_t));
      tc->tc_qm = qm;
      for (fld_ctr = 0; fld_ctr < SPART_TRIPLE_FIELDS_COUNT; fld_ctr++)
        {
          if (NULL != tcc->tcc_cuts [fld_ctr])
            tc->tc_red_cuts [fld_ctr] = (ccaddr_t *)t_revlist_to_array (tcc->tcc_cuts [fld_ctr]);
      else
            tc->tc_red_cuts [fld_ctr] = NULL;
    }
      tcc->tcc_nonfiltered_cases_found += 1;
      if (inside_allowed_qm)
        dk_set_push (&(tcc->tcc_found_cases), tc);
    }
  if (SPART_QM_EXCLUSIVE & qm->qmMatchingFlags)
    {
      for (fld_ctr = 0; fld_ctr < SPART_TRIPLE_FIELDS_COUNT; fld_ctr++)
        {
          caddr_t fld_const = SPARP_FIELD_CONST_OF_QM (qm, fld_ctr);
          qm_value_t *qmv = SPARP_FIELD_QMV_OF_QM (qm, fld_ctr);
          if (NULL != fld_const)
            {
              if (-1 == single_fixed_fld)
                {
                  single_fixed_fld = fld_ctr;
                  continue;
                }
              single_fixed_fld = -2;
              break;
            }
          if (NULL != qmv)
            {
              single_fixed_fld = -3;
              break;
                }
        }
      if (0 <= single_fixed_fld)
        {
          caddr_t single_fixed_val = SPARP_FIELD_CONST_OF_QM (qm, single_fixed_fld);
          dk_set_push (tcc->tcc_cuts + single_fixed_fld, single_fixed_val);
        }
    }
  if ((SSG_QM_PROVEN_MATCH == common_status) && (SPART_QM_SOFT_EXCLUSIVE & qm->qmMatchingFlags))
    return SSG_QM_MATCH_AND_CUT;
  if ((SSG_QM_APPROX_MATCH <= common_status) && (SPART_QM_EXCLUSIVE & qm->qmMatchingFlags))
    return SSG_QM_MATCH_AND_CUT;
  return common_status;
}

triple_case_t **
sparp_find_triple_cases (sparp_t *sparp, SPART *triple, SPART **sources, int required_source_type)
{
  int ctr, fld_ctr, source_ctr;
  triple_case_t **res_list;
  tc_context_t tmp_tcc;
  if (NULL == sparp->sparp_storage)
    {
      triple_case_t **res_list = (triple_case_t **)t_list (1, tc_default);
      mp_box_tag_modify (res_list, DV_ARRAY_OF_LONG);
      return res_list;
    }
  memset (&tmp_tcc, 0, sizeof (tc_context_t));
  tmp_tcc.tcc_triple = triple;
  tmp_tcc.tcc_qs = sparp->sparp_storage;
  tmp_tcc.tcc_sources = sources;
  tmp_tcc.tcc_source_invalidation_masks = (uint32 *)t_alloc_box (sizeof (uint32) * BOX_ELEMENTS (sources), DV_BIN);
  DO_BOX_FAST (SPART *, source, source_ctr, tmp_tcc.tcc_sources)
        {
      if (required_source_type == SPART_TYPE(source))
            {
          tmp_tcc.tcc_check_source_graphs++;
          tmp_tcc.tcc_source_invalidation_masks[source_ctr] = 0;
    }
  else
        tmp_tcc.tcc_source_invalidation_masks[source_ctr] = 0x1;
    }
  END_DO_BOX_FAST;
  if (((caddr_t)DEFAULT_L) == triple->_.triple.qm_iri)
    {
      if (NULL == sparp->sparp_storage->qsDefaultMap)
        spar_error (sparp, "QUAD MAP DEFAULT group pattern is used in RDF storage that has no default quad map");
      tmp_tcc.tcc_top_allowed_qm = sparp->sparp_storage->qsDefaultMap;
    }
  else if (((caddr_t)_STAR) == triple->_.triple.qm_iri)
    tmp_tcc.tcc_top_allowed_qm = NULL;
  else
    {
      quad_map_t *top_qm = sparp_find_quad_map_by_name (triple->_.triple.qm_iri);
      if (NULL == top_qm)
        spar_error (sparp, "QUAD MAP '%.200s' group pattern refers to undefined quad map", triple->_.triple.qm_iri);
      tmp_tcc.tcc_top_allowed_qm = top_qm;
    }
  DO_BOX_FAST (quad_map_t *, qm, ctr, sparp->sparp_storage->qsMjvMaps)
    {
      int status;
      if (0 != BOX_ELEMENTS_0 (qm->qmUserSubMaps))
        spar_internal_error (sparp, "RDF quad mapping metadata are corrupted: MJV has submaps; the quad storage used in the query should be configured again");
      if (SPART_QM_EMPTY & qm->qmMatchingFlags)
        spar_internal_error (sparp, "RDF quad mapping metadata are corrupted: MJV is declared as empty; the quad storage used in the query should be configured again");
      status = sparp_qm_find_triple_cases (sparp, &tmp_tcc, qm, 0, 1);
      if (SSG_QM_MATCH_AND_CUT == status)
        goto full_exclusive_match_detected;
    }
  END_DO_BOX_FAST;
  DO_BOX_FAST (quad_map_t *, qm, ctr, sparp->sparp_storage->qsUserMaps)
    {
      int status = sparp_qm_find_triple_cases (sparp, &tmp_tcc, qm, 0, 1);
      if (SSG_QM_MATCH_AND_CUT == status)
        goto full_exclusive_match_detected;
    }
  END_DO_BOX_FAST;
  if (NULL != sparp->sparp_storage->qsDefaultMap)
    sparp_qm_find_triple_cases (sparp, &tmp_tcc, sparp->sparp_storage->qsDefaultMap, 0, 1);

full_exclusive_match_detected:
#if 0
  if (NULL == tmp_tcc.tcc_found_cases)
    spar_internal_error (sparp, "Empty quad map list :(");
#else
#ifdef DEBUG
  if (NULL == tmp_tcc.tcc_found_cases)
    {
      printf ("Empty quad map list:");
      printf ("\nStorage : "); dbg_print_box (sparp->sparp_env->spare_storage_name, stdout);
      printf ("\nGraph   : "); dbg_print_box ((caddr_t)(triple->_.triple.tr_graph), stdout);
      printf ("\nSubj    : "); dbg_print_box ((caddr_t)(triple->_.triple.tr_subject), stdout);
      printf ("\nPred    : "); dbg_print_box ((caddr_t)(triple->_.triple.tr_predicate), stdout);
      printf ("\nObj     : "); dbg_print_box ((caddr_t)(triple->_.triple.tr_object), stdout);
      printf ("\n");
    }
#endif
#endif
  res_list = (triple_case_t **)t_revlist_to_array (tmp_tcc.tcc_found_cases);
  mp_box_tag_modify (res_list, DV_ARRAY_OF_LONG);
  while (tmp_tcc.tcc_found_cases) dk_set_pop (&(tmp_tcc.tcc_found_cases));
  for (fld_ctr = 0; fld_ctr < SPART_TRIPLE_FIELDS_COUNT; fld_ctr++)
    {
      dk_set_t cuts = tmp_tcc.tcc_cuts[fld_ctr];
      while (cuts) dk_set_pop (&cuts);
    }
  return res_list;
}

void
sparp_refresh_triple_cases (sparp_t *sparp, SPART *triple)
{
  SPART **sources = sparp->sparp_expr->_.req_top.sources;
  /*ssg_valmode_t valmodes[SPART_TRIPLE_FIELDS_COUNT];*/
  triple_case_t **new_cases;
  int old_cases_count, new_cases_count, ctr;
  int field_ctr;
  SPART *graph;
  int required_source_type;
  old_cases_count = BOX_ELEMENTS_0 (triple->_.triple.tc_list);
  if ((0 < old_cases_count) && (4 > old_cases_count))
    return;
  graph = triple->_.triple.tr_graph;
  required_source_type = ((SPAR_VARIABLE == SPART_TYPE(graph)) ? NAMED_L : FROM_L);
  new_cases = sparp_find_triple_cases (sparp, triple, sources, required_source_type);
  new_cases_count = BOX_ELEMENTS (new_cases);
  if ((NULL == triple->_.triple.tc_list) &&
    (0 == new_cases_count) &&
    sparp->sparp_env->spare_signal_void_variables )
    spar_error (sparp, "No one quad map pattern is suitable for GRAPH %s { %s %s %s } triple at line %d",
      spar_dbg_string_of_triple_field (sparp, graph),
      spar_dbg_string_of_triple_field (sparp, triple->_.triple.tr_subject),
      spar_dbg_string_of_triple_field (sparp, triple->_.triple.tr_predicate),
      spar_dbg_string_of_triple_field (sparp, triple->_.triple.tr_object),
      triple->srcline );
  for (field_ctr = SPART_TRIPLE_FIELDS_COUNT; field_ctr--; /*no step*/)
    {
      ssg_valmode_t field_valmode = SSG_VALMODE_AUTO;
      SPART *field_expn = triple->_.triple.tr_fields[field_ctr];
      rdf_val_range_t acc_rvr;
      int all_cases_make_only_refs = 1;
      memset (&acc_rvr, 0, sizeof (rdf_val_range_t));
      acc_rvr.rvrRestrictions = SPART_VARR_CONFLICT;
      for (ctr = 0; ctr < new_cases_count; ctr++)
        {
          triple_case_t *tc = new_cases [ctr];
          qm_value_t *qmv = SPARP_FIELD_QMV_OF_QM (tc->tc_qm, field_ctr);
          caddr_t fld_const = SPARP_FIELD_CONST_OF_QM (tc->tc_qm, field_ctr);
          ccaddr_t *red_cuts = tc->tc_red_cuts [field_ctr];
          rdf_val_range_t qmv_rvr;
          if (NULL != qmv)
            {
              qm_format_t *qmv_fmt = qmv->qmvFormat;
              if (all_cases_make_only_refs && !(SPART_VARR_IS_REF & qmv_fmt->qmfValRange.rvrRestrictions))
                all_cases_make_only_refs = 0;
              field_valmode = ssg_smallest_union_valmode (field_valmode, qmv_fmt);
              sparp_rvr_copy (sparp, &qmv_rvr, &(qmv->qmvRange));
              if (SPART_VARR_SPRINTFF & qmv_fmt->qmfValRange.rvrRestrictions)
                {
                  qmv_rvr.rvrRestrictions |= SPART_VARR_SPRINTFF;
                  sparp_rvr_add_sprintffs (sparp, &qmv_rvr, qmv_fmt->qmfValRange.rvrSprintffs, qmv_fmt->qmfValRange.rvrSprintffCount);
                }
              if ((NULL != qmv->qmvIriClass) &&
                !(SPART_VARR_IRI_CALC & qmv_rvr.rvrRestrictions) &&
                (SPART_VARR_IS_REF & qmv->qmvFormat->qmfValRange.rvrRestrictions) )
                {
                  qmv_rvr.rvrRestrictions |= SPART_VARR_IRI_CALC;
                  sparp_rvr_add_iri_classes (sparp, &qmv_rvr, &(qmv->qmvIriClass), 1);
                }
              sparp_rvr_intersect_red_cuts (sparp, &qmv_rvr, red_cuts, BOX_ELEMENTS_0 (red_cuts));
            }
          else if (NULL != fld_const)
            {
              if (NULL != qmv)
                spar_internal_error (sparp, "Invalid quad map storage metadata: quad map has set both quad map value and a constant for same field.");
              memset (&qmv_rvr, 0, sizeof (rdf_val_range_t));
              qmv_rvr.rvrRestrictions |= SPART_VARR_FIXED;
#ifdef DEBUG
              if ((SPART_TRIPLE_GRAPH_IDX == field_ctr) && (DV_UNAME != DV_TYPE_OF (fld_const)))
                GPF_T1("sparp_" "refresh_triple_cases(): const GRAPH field of qm is not a UNAME");
#endif
              qmv_rvr.rvrFixedValue = fld_const;
              if (all_cases_make_only_refs && (DV_UNAME != DV_TYPE_OF (fld_const)))
                all_cases_make_only_refs = 0;
    }
          else
            spar_internal_error (sparp, "Invalid quad map storage metadata: neither quad map value nor a constant is set for a field of a quad map.");
          if ((NULL != qmv_rvr.rvrFixedValue) && !(SPART_VARR_FIXED & qmv_rvr.rvrRestrictions))
    {
              if (DV_UNAME == DV_TYPE_OF (qmv_rvr.rvrFixedValue))
                qmv_rvr.rvrRestrictions |= (SPART_VARR_FIXED | SPART_VARR_IS_REF | SPART_VARR_IS_IRI | SPART_VARR_NOT_NULL);
              else
                qmv_rvr.rvrRestrictions |= (SPART_VARR_FIXED | SPART_VARR_IS_LIT | SPART_VARR_NOT_NULL);
    }
          if (NULL != qmv)
            sparp_rvr_tighten (sparp, &qmv_rvr, &(qmv->qmvFormat->qmfValRange), ~SPART_VARR_IRI_CALC);
          sparp_rvr_loose (sparp, &acc_rvr, &qmv_rvr, ~0);
        }
      if (all_cases_make_only_refs && (SSG_VALMODE_LONG == field_valmode))
        field_valmode = SSG_VALMODE_SQLVAL;
      sparp_jso_validate_format (sparp, field_valmode);
      triple->_.triple.native_formats[field_ctr] = field_valmode;
      if (SPAR_IS_BLANK_OR_VAR (field_expn))
        sparp_rvr_tighten (sparp, &(field_expn->_.var.rvr), &acc_rvr, ~0);
    }
  triple->_.triple.tc_list = new_cases;
}

void
sparp_flatten_union (sparp_t *sparp, SPART *parent_gp)
{
  int memb_ctr;
#ifdef DEBUG
  if (SPAR_GP != SPART_TYPE (parent_gp))
    spar_internal_error (sparp, "sparp_" "flatten_union(): parent_gp is not a GP");
  if (UNION_L != parent_gp->_.gp.subtype)
    spar_internal_error (sparp, "sparp_" "flatten_union(): parent_gp is not a union");
#endif
  for (memb_ctr = BOX_ELEMENTS (parent_gp->_.gp.members); memb_ctr--; /*no step*/)
    {
      SPART *memb = parent_gp->_.gp.members [memb_ctr];
      if ((SPAR_GP == SPART_TYPE (memb)) &&
        ((UNION_L == memb->_.gp.subtype) ||
         ((0 == memb->_.gp.subtype) &&
           (1 == BOX_ELEMENTS (memb->_.gp.members)) &&
           (SPAR_GP == SPART_TYPE (memb->_.gp.members[0])) ) ) )
        {
          int sub_count = BOX_ELEMENTS (memb->_.gp.members);
          int sub_ctr;
          SPART **memb_filters = sparp_gp_detach_all_filters (sparp, memb, NULL);
          int memb_filters_count = BOX_ELEMENTS_0 (memb_filters);
          for (sub_ctr = sub_count; sub_ctr--; /* no step */)
            {
              SPART *sub_memb = sparp_gp_detach_member (sparp, memb, sub_ctr, NULL);
              sparp_gp_attach_member (sparp, parent_gp, sub_memb, memb_ctr, NULL);
              if (0 != memb_filters_count)
                sparp_gp_attach_many_filters (sparp, sub_memb, sparp_treelist_full_copy (sparp, memb_filters, NULL), 0, NULL);
            }
          memb_ctr += sub_count;
          sparp_gp_detach_member (sparp, parent_gp, memb_ctr, NULL);
        }
    }
}


void
sparp_flatten_join (sparp_t *sparp, SPART *parent_gp)
{
  int memb_ctr;
#ifdef DEBUG
  if (SPAR_GP != SPART_TYPE (parent_gp))
    spar_internal_error (sparp, "sparp_" "flatten_join(): parent_gp is not a GP");
  if ((UNION_L == parent_gp->_.gp.subtype) || (SELECT_L == parent_gp->_.gp.subtype))
    spar_internal_error (sparp, "sparp_" "flatten_join(): parent_gp is not a join");
#endif
  for (memb_ctr = BOX_ELEMENTS (parent_gp->_.gp.members); memb_ctr--; /*no step*/)
    {
      SPART *memb = parent_gp->_.gp.members [memb_ctr];
      if (SPAR_GP != SPART_TYPE (memb))
        continue;
      if (OPTIONAL_L == memb->_.gp.subtype)
        break; /* No optimizations at left of LEFT OUTER JOIN. */
      if (((0 == memb->_.gp.subtype) &&
           (1 <= BOX_ELEMENTS (memb->_.gp.members)) ) ||
         ((UNION_L == memb->_.gp.subtype) &&
          (1 == BOX_ELEMENTS (memb->_.gp.members)) ) )
        {
          int sub_count = BOX_ELEMENTS (memb->_.gp.members);
          int sub_ctr;
          SPART **memb_filters = sparp_gp_detach_all_filters (sparp, memb, NULL);
          int memb_filters_count = BOX_ELEMENTS_0 (memb_filters);
          for (sub_ctr = sub_count; sub_ctr--; /* no step */)
            {
              SPART *sub_memb = sparp_gp_detach_member (sparp, memb, sub_ctr, NULL);
              sparp_gp_attach_member (sparp, parent_gp, sub_memb, memb_ctr, NULL);
            }
          if (0 != memb_filters_count)
            sparp_gp_attach_many_filters (sparp, parent_gp, sparp_treelist_full_copy (sparp, memb_filters, NULL), 0, NULL);
          memb_ctr += sub_count;
          sparp_gp_detach_member (sparp, parent_gp, memb_ctr, NULL);
        }
    }
}

void
sparp_set_triple_selid_and_tabid (sparp_t *sparp, SPART *triple, caddr_t new_selid, caddr_t new_tabid)
{
  int field_ctr;
  if (
    !strcmp (triple->_.triple.tabid, new_tabid) &&
    !strcmp (triple->_.triple.selid, new_selid) )
    return;
  for (field_ctr = SPART_TRIPLE_FIELDS_COUNT; field_ctr--; /*no step*/)
    {
      SPART *fld_expn = triple->_.triple.tr_fields[field_ctr];
      if (!SPAR_IS_BLANK_OR_VAR (fld_expn))
        continue;
      if (strcmp (fld_expn->_.var.selid, new_selid)) /* weird re-location */
        {
          if (SPART_BAD_EQUIV_IDX != fld_expn->_.var.equiv_idx)
            {
              sparp_equiv_t *eq = SPARP_EQUIV (sparp, fld_expn->_.var.equiv_idx);
              sparp_equiv_remove_var (sparp, eq, fld_expn);
            }
          fld_expn->_.var.selid = t_box_copy (new_selid);
        }
      fld_expn->_.var.tabid = t_box_copy (new_tabid);
    }
  triple->_.triple.selid = t_box_copy (new_selid);
  triple->_.triple.tabid = t_box_copy (new_tabid);
}


#if 0
int
sparp_set_retval_selid_cbk (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  if (SPAR_IS_BLANK_OR_VAR (curr))
    curr->_.var.selid = t_box_copy (common_env);
  return 0;
}

void
sparp_set_retval_and_order_selid (sparp_t *sparp)
{
  int ctr;
  sparp_trav_state_t stss [SPARP_MAX_SYNTDEPTH+2];
  caddr_t top_gp_selid = sparp->sparp_expr->_.req_top.pattern->_.gp.selid;
  DO_BOX_FAST (SPART *, filt, ctr, sparp->sparp_expr->_.req_top.retvals)
    {
      sparp_gp_trav_int (sparp, filt, stss + 1, top_gp_selid,
        NULL, NULL,
        sparp_set_retval_selid_cbk, NULL, NULL,
        NULL );
    }
  END_DO_BOX_FAST;
  DO_BOX_FAST (SPART *, grouping, ctr, sparp->sparp_expr->_.req_top.groupings)
    {
      sparp_gp_trav_int (sparp, grouping, stss + 1, top_gp_selid,
        NULL, NULL,
        sparp_set_retval_selid_cbk, NULL, NULL,
        NULL );
    }
  END_DO_BOX_FAST;
  DO_BOX_FAST (SPART *, oby, ctr, sparp->sparp_expr->_.req_top.order)
    {
      sparp_gp_trav_int (sparp, oby->_.oby.expn, stss + 1, top_gp_selid,
        NULL, NULL,
        sparp_set_retval_selid_cbk, NULL, NULL,
        NULL );
    }
  END_DO_BOX_FAST;
}
#endif

int
sparp_expns_are_equal (sparp_t *sparp, SPART *one, SPART *two)
{
  ptrlong one_type = SPART_TYPE (one);
  if (one == two)
    return 1;
  if (SPART_TYPE (two) != one_type)
    return 0;
  switch (one_type)
    {
    case SPAR_BLANK_NODE_LABEL: case SPAR_VARIABLE:
      return !strcmp (one->_.var.vname, two->_.var.vname);
    case SPAR_QNAME: /* case SPAR_QNAME_NS: */
      return sparp_expns_are_equal (sparp, (SPART *)(one->_.lit.val), (SPART *)(two->_.lit.val));
    case SPAR_LIT:
      if (DV_TYPE_OF (one) != DV_TYPE_OF (two))
        return 0;
      if (DV_ARRAY_OF_POINTER != DV_TYPE_OF (one))
        return (DVC_MATCH == cmp_boxes ((caddr_t)one, (caddr_t)two, NULL, NULL));
      else
        return (
          sparp_expns_are_equal (sparp, (SPART *)(one->_.lit.val), (SPART *)(two->_.lit.val)) &&
          sparp_expns_are_equal (sparp, (SPART *)(one->_.lit.datatype), (SPART *)(two->_.lit.datatype)) &&
          sparp_expns_are_equal (sparp, (SPART *)(one->_.lit.language), (SPART *)(two->_.lit.language)) );
    case SPAR_BUILT_IN_CALL:
      return (
        (one->_.builtin.btype == two->_.builtin.btype) &&
        sparp_expn_lists_are_equal (sparp, one->_.builtin.args, two->_.builtin.args) );
    case SPAR_FUNCALL:
      return (
        (one->_.funcall.agg_mode == two->_.funcall.agg_mode) &&
        !strcmp (one->_.funcall.qname, two->_.funcall.qname) &&
        sparp_expn_lists_are_equal (sparp, one->_.funcall.argtrees, two->_.funcall.argtrees) );
    case BOP_EQ: case BOP_NEQ:
    case BOP_AND: case BOP_OR:
    case BOP_SAME: case BOP_NSAME:
      return (
        ( sparp_expns_are_equal (sparp, one->_.bin_exp.left, two->_.bin_exp.left) &&
          sparp_expns_are_equal (sparp, one->_.bin_exp.right, two->_.bin_exp.right) ) ||
        ( sparp_expns_are_equal (sparp, one->_.bin_exp.left, two->_.bin_exp.right) &&
          sparp_expns_are_equal (sparp, one->_.bin_exp.right, two->_.bin_exp.left) ) );
    case BOP_LT: case BOP_LTE: case BOP_GT: case BOP_GTE:
    /*case BOP_LIKE: Like is built-in in SPARQL, not a BOP! */
    case BOP_PLUS: case BOP_MINUS: case BOP_TIMES: case BOP_DIV: case BOP_MOD:
      return (
        sparp_expns_are_equal (sparp, one->_.bin_exp.left, two->_.bin_exp.left) &&
        sparp_expns_are_equal (sparp, one->_.bin_exp.right, two->_.bin_exp.right) );
    case BOP_NOT:
      return sparp_expns_are_equal (sparp, one->_.bin_exp.left, two->_.bin_exp.left);
/* Add more cases right above this line when introducing more SPAR_nnn constants that may appear inside expression */
    default: spar_internal_error (sparp, "sparp_" "expns_are_equal () get expression of unsupported type");
    }
  GPF_T;
  return 0;
}

int
sparp_expn_lists_are_equal (sparp_t *sparp, SPART **one, SPART **two)
{
  int ctr, one_len = BOX_ELEMENTS_0 (one);
  if (BOX_ELEMENTS_0 (two) != one_len)
    return 0;
  for (ctr = 0; ctr < one_len; ctr++)
    {
      if (!sparp_expns_are_equal (sparp, one[ctr], two[ctr]))
        return 0;
    }
  return 1;
}

#if 0
int
sparp_set_special_order_selid_cbk (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  SPART *new_gp = (SPART *)common_env;
  if (SPAR_IS_BLANK_OR_VAR (curr))
    {
      sparp_equiv_t *eq;
/* !!! TBD: replace with silent detach if needed.
      int idx = curr->_.var.equiv_idx;
      if (SPART_BAD_EQUIV_IDX != idx)
        spar_internal_error (sparp, "sparp_" "set_special_order_selid(): attempt to attach a filter with used variable"); */
      curr->_.var.selid = t_box_copy (new_gp->_.gp.selid);
      eq = sparp_equiv_get (sparp, new_gp, curr, SPARP_EQUIV_INS_VARIABLE/* !!!TBD: add | SPARP_EQUIV_ADD_CONST_READ*/);
      if (NULL == eq)
        spar_internal_error (sparp, "sparp_" "set_special_order_selid(): variable in order by comes from nowhere");
      curr->_.var.equiv_idx = eq->e_own_idx;
    }
  return 0;
}

void
sparp_set_special_order_selid (sparp_t *sparp, SPART *new_gp)
{
  int ctr;
  sparp_trav_state_t stss [SPARP_MAX_SYNTDEPTH+2];
  DO_BOX_FAST (SPART *, oby, ctr, sparp->sparp_expr->_.req_top.order)
    {
      sparp_gp_trav_int (sparp, oby->_.oby.expn, stss + 1, new_gp,
        NULL, NULL,
        sparp_set_special_order_selid_cbk, NULL, NULL,
        NULL );
    }
  END_DO_BOX_FAST;
}
#endif

SPART **
sparp_make_qm_cases (sparp_t *sparp, SPART *triple)
{
  triple_case_t **tc_list = triple->_.triple.tc_list;
  SPART **res;
  int tc_idx;
#ifdef DEBUG
  if (1 >= BOX_ELEMENTS (triple->_.triple.tc_list))
    spar_internal_error (sparp, "sparp_" "make_qm_cases(): redundant call");
#endif
  res = (SPART **)t_alloc_box (box_length (tc_list), DV_ARRAY_OF_POINTER);
  DO_BOX_FAST (triple_case_t *, tc, tc_idx, tc_list)
    {
      quad_map_t * qm = tc->tc_qm;
      int field_ctr;
      SPART *qm_case_triple = (SPART *)t_box_copy ((caddr_t)triple);
      SPART *qm_case_gp = sparp_new_empty_gp (sparp, 0, unbox (triple->srcline));
      caddr_t qm_selid = qm_case_gp->_.gp.selid;
      caddr_t qm_tabid = t_box_sprintf (100, "%s-qm%d", triple->_.triple.tabid, tc_idx);
      triple_case_t **one_tc = (triple_case_t **)t_list (1, tc);
      mp_box_tag_modify (one_tc, DV_ARRAY_OF_LONG);
      qm_case_triple->_.triple.tc_list = one_tc;
      qm_case_triple->_.triple.selid = t_box_copy (qm_selid);
      qm_case_triple->_.triple.tabid = qm_tabid;
      for (field_ctr = SPART_TRIPLE_FIELDS_COUNT; field_ctr--; /*no step*/)
        {
          SPART *fld_expn = triple->_.triple.tr_fields[field_ctr];
          qm_value_t *fld_qmv = SPARP_FIELD_QMV_OF_QM (qm,field_ctr);
          caddr_t fld_const = SPARP_FIELD_CONST_OF_QM (qm,field_ctr);
          ccaddr_t *fld_tc_cuts = tc->tc_red_cuts [field_ctr];
          SPART *new_fld_expn;
          qm_format_t *native_fmt;
          if (SPAR_IS_BLANK_OR_VAR (fld_expn))
            {
              sparp_equiv_t *eq;
              new_fld_expn = (SPART *)t_box_copy ((caddr_t)fld_expn);
              new_fld_expn->_.var.selid = t_box_copy (qm_selid);
              new_fld_expn->_.var.tabid = t_box_copy (qm_tabid);
              new_fld_expn->_.var.vname = t_box_copy (fld_expn->_.var.vname);
              new_fld_expn->_.var.equiv_idx = SPART_BAD_EQUIV_IDX;
              sparp_rvr_copy (sparp, &(new_fld_expn->_.var.rvr), &(fld_expn->_.var.rvr)); 
              eq = sparp_equiv_get (sparp, qm_case_gp, new_fld_expn, SPARP_EQUIV_INS_CLASS | SPARP_EQUIV_INS_VARIABLE | SPARP_EQUIV_ADD_GPSO_USE);
              if (NULL == fld_qmv)
                sparp_equiv_restrict_by_constant (sparp, eq, NULL, (SPART *)fld_const);
              else
                {
                  sparp_equiv_tighten (sparp, eq, &(fld_qmv->qmvRange), ~SPART_VARR_IRI_CALC);
                  sparp_equiv_tighten (sparp, eq, &(fld_qmv->qmvFormat->qmfValRange), ~SPART_VARR_IRI_CALC);
                }
              if (NULL != fld_tc_cuts)
                sparp_rvr_add_red_cuts (sparp, &(eq->e_rvr), fld_tc_cuts, BOX_ELEMENTS (fld_tc_cuts));
              sparp_rvr_tighten (sparp, (&new_fld_expn->_.var.rvr), &(eq->e_rvr), ~0);
            }
          else
            new_fld_expn = sparp_tree_full_copy (sparp, fld_expn, NULL);
          qm_case_triple->_.triple.tr_fields[field_ctr] = new_fld_expn;
          native_fmt = ((NULL != fld_qmv) ? fld_qmv->qmvFormat : SSG_VALMODE_AUTO);
          sparp_jso_validate_format (sparp, native_fmt);
          qm_case_triple->_.triple.native_formats[field_ctr] = native_fmt;
        }
      sparp_gp_attach_member (sparp, qm_case_gp, qm_case_triple, 0, NULL);
      res [tc_idx] = qm_case_gp;
    }
  END_DO_BOX_FAST;
  return res;
}

SPART *
sparp_new_empty_gp (sparp_t *sparp, ptrlong subtype, ptrlong srcline)
{
  SPART *res = spartlist (sparp, 8,
    SPAR_GP, subtype,
    t_list (0),
    t_list (0),
    NULL,
    spar_mkid (sparp, "s"),
    NULL, (ptrlong)(0) );
  res->srcline = t_box_num (srcline);
  return res;
}

void
sparp_gp_produce_nothing (sparp_t *sparp, SPART *curr)
{
  int eq_ctr;
  SPARP_REVFOREACH_GP_EQUIV (sparp, curr, eq_ctr, eq)
    {
      int recv_eq_ctr;
      switch (curr->_.gp.subtype)
        {
          case OPTIONAL_L:
            eq->e_rvr.rvrRestrictions |= SPART_VARR_ALWAYS_NULL;
            continue;
          case UNION_L:
            eq->e_rvr.rvrRestrictions |= SPART_VARR_CONFLICT;
            continue;
        }
      DO_BOX_FAST (ptrlong, recv_eq_idx, recv_eq_ctr, eq->e_receiver_idxs)
        {
          sparp_equiv_t *recv_eq = SPARP_EQUIV (sparp, recv_eq_idx);
          if (UNION_L != recv_eq->e_gp->_.gp.subtype)
          recv_eq->e_rvr.rvrRestrictions |= SPART_VARR_CONFLICT;
          sparp_equiv_disconnect (sparp, recv_eq, eq);
        }
      END_DO_BOX_FAST;
    }
  END_SPARP_REVFOREACH_GP_EQUIV;
  sparp_gp_detach_all_filters (sparp, curr, NULL);
  while (0 < BOX_ELEMENTS (curr->_.gp.members))
    sparp_gp_detach_member (sparp, curr, 0, NULL);
}

int sparp_gp_trav_refresh_triple_cases (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  if (SPAR_TRIPLE != curr->type) /* Not a triple ? -- nothing to do */
    return 0;
  sparp_refresh_triple_cases (sparp, curr);
  return SPAR_GPT_NODOWN /*| SPAR_GPT_NOOUT */;
}


int sparp_gp_trav_multiqm_to_unions (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  int memb_ctr;
  if (SPAR_GP != curr->type) /* Not a gp ? -- nothing to do */
    return 0;
  DO_BOX_FAST_REV (SPART *, memb, memb_ctr, curr->_.gp.members)
    { /* countdown direction of 'for' is important due to possible insertions/removals */
      int tc_count;
      SPART **qm_cases;
      int case_ctr;
      if (SPAR_TRIPLE != memb->type)
        continue;
      tc_count = BOX_ELEMENTS (memb->_.triple.tc_list);
      if (1 == tc_count)
        continue;
      if (0 == tc_count)
        {
          if (UNION_L != curr->_.gp.subtype)
            {
              sparp_gp_produce_nothing (sparp, curr);
              return SPAR_GPT_NODOWN;
            }
          sparp_gp_detach_member (sparp, curr, memb_ctr, NULL);
          continue;
        }
      sparp_gp_detach_member (sparp, curr, memb_ctr, NULL);
      qm_cases = sparp_make_qm_cases (sparp, memb);
      if (UNION_L == curr->_.gp.subtype)
        {
          DO_BOX_FAST_REV (SPART *, qm_case, case_ctr, qm_cases)
            {
              sparp_gp_attach_member (sparp, curr, qm_case, memb_ctr, NULL);
            }
          END_DO_BOX_FAST_REV;
        }
      else
        {
          SPART *case_union = sparp_new_empty_gp (sparp, UNION_L, unbox (memb->srcline));
          DO_BOX_FAST_REV (SPART *, qm_case, case_ctr, qm_cases)
            {
              sparp_gp_attach_member (sparp, case_union, qm_case, 0, NULL);
            }
          END_DO_BOX_FAST_REV;
          sparp_gp_attach_member (sparp, curr, case_union, memb_ctr, NULL);
        }
    }
  END_DO_BOX_FAST_REV;
  if (UNION_L == curr->_.gp.subtype)
    sparp_flatten_union (sparp, curr);
  else if (SELECT_L != curr->_.gp.subtype)
    sparp_flatten_join (sparp, curr);
  return 0;
}


int sparp_gp_trav_detach_conflicts_out (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  int memb_ctr;
  if (SPAR_GP != curr->type) /* Not a gp ? -- nothing to do */
    return 0;
  DO_BOX_FAST_REV (SPART *, memb, memb_ctr, curr->_.gp.members)
    { /* countdown direction of 'for' is important due to possible removals */
      int eq_ctr;
      if (SPAR_GP != memb->type)
        continue;
      if (OPTIONAL_L == memb->_.gp.subtype)
        continue;
      if ((UNION_L == memb->_.gp.subtype) && (0 == BOX_ELEMENTS_0 (memb->_.gp.members)))
        {
          goto do_detach; /* see below */
        }
      SPARP_FOREACH_GP_EQUIV (sparp, memb, eq_ctr, eq)
        {
          if ((SPART_VARR_CONFLICT & eq->e_rvr.rvrRestrictions) &&
            (SPART_VARR_NOT_NULL & eq->e_rvr.rvrRestrictions) )
            goto do_detach; /* see below */
        }
      END_SPARP_FOREACH_GP_EQUIV;
      continue;

do_detach:
      if (UNION_L != curr->_.gp.subtype)
        {
          sparp_gp_produce_nothing (sparp, curr);
          return SPAR_GPT_NODOWN;
        }
      sparp_gp_detach_member (sparp, curr, memb_ctr, NULL);
    }
  END_DO_BOX_FAST_REV;
  if (UNION_L == curr->_.gp.subtype)
    sparp_flatten_union (sparp, curr);
  else if (SELECT_L != curr->_.gp.subtype)
    sparp_flatten_join (sparp, curr);
  return 0;
}

int sparp_gp_trav_1var (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  SPART **single_var_ptr = (SPART **)common_env;
  if (!SPAR_IS_BLANK_OR_VAR (curr))
    return 0;
  if (SPART_VARNAME_IS_GLOB (curr->_.var.vname))
    return 0;
  if (NULL != single_var_ptr[0])
    {
      single_var_ptr[0] = (SPART *)(1L);
      return SPAR_GPT_COMPLETED;
    }
  single_var_ptr[0] = curr;
  return SPAR_GPT_NODOWN;
}

int sparp_gp_trav_localize_filters (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  int filt_ctr;
  if (SPAR_GP != curr->type) /* Not a gp ? -- nothing to do */
    return 0;
  if (0 == BOX_ELEMENTS_0 (curr->_.gp.filters)) /* No filters -- nothing to do */
    return 0;
  DO_BOX_FAST_REV (SPART *, filt, filt_ctr, curr->_.gp.filters)
    {
      SPART *single_var = NULL;
      sparp_equiv_t *sv_eq;
      int subval_ctr, subval_count;
      sparp_gp_trav_int (sparp, filt, sts_this, &(single_var),
        NULL, NULL,
        sparp_gp_trav_1var, NULL, NULL,
        NULL );
      if (!IS_BOX_POINTER (single_var))
        continue;
      sv_eq = SPARP_EQUIV(sparp, single_var->_.var.equiv_idx);
      if (!(SPART_VARR_NOT_NULL & sv_eq->e_rvr.rvrRestrictions))
        continue; /* It's unsafe to localize a filter on nullable variable. Consider { ... optional {<s> <p> ?o} filter (!bound(?o))}} */
      if (0 != sv_eq->e_gspo_uses)
        continue; /* The filter can not be detached here so it may be localized on every loop, resulting in redundand localized filters */
      subval_count = BOX_ELEMENTS_0 (sv_eq->e_subvalue_idxs);
      if (0 == subval_count)
        continue; /* No subvalues -- can't localize because this either have no effect or drop the filter */
      if (10 < subval_count)
        continue; /* Too many subvalues -- unsafe to localize, too many filter conditions may kill SQL compiler */
      DO_BOX_FAST_REV (ptrlong, subval_eq_idx, subval_ctr, sv_eq->e_subvalue_idxs)
        {
          sparp_equiv_t *sub_eq = SPARP_EQUIV (sparp, subval_eq_idx);
          SPART *sub_gp = sub_eq->e_gp;
          if (UNION_L == sub_gp->_.gp.subtype)
            subval_count += (BOX_ELEMENTS (sub_gp->_.gp.members) - 1);
        }
      END_DO_BOX_FAST_REV;
      if (10 < subval_count)
        continue; /* Too many subvalues found after correction for member unions */
      sparp_gp_detach_filter (sparp, curr, filt_ctr, NULL);
      DO_BOX_FAST_REV (ptrlong, subval_eq_idx, subval_ctr, sv_eq->e_subvalue_idxs)
        {
          sparp_equiv_t *sub_eq = SPARP_EQUIV (sparp, subval_eq_idx);
          SPART *sub_gp = sub_eq->e_gp;
          SPART *filter_clone = sparp_tree_full_copy (sparp, filt, curr);
          sparp_gp_attach_filter (sparp, sub_gp, filter_clone, 0, NULL);
        }
      END_DO_BOX_FAST_REV;
    }
  END_DO_BOX_FAST_REV;
  return 0;
}

int
sparp_calc_importance_of_eq (sparp_t *sparp, sparp_equiv_t *eq)
{
  int res = 16 * eq->e_subquery_uses +
    4 * eq->e_gspo_uses +
    4 * eq->e_nested_bindings +
    3 * eq->e_optional_reads +
    2 * eq->e_const_reads +
    2 * BOX_ELEMENTS_0 (eq->e_receiver_idxs);
  if (SPART_VARR_FIXED & eq->e_rvr.rvrRestrictions)
    res *= 4;
  if (SPART_VARR_TYPED & eq->e_rvr.rvrRestrictions)
    res *= 2;
  if ((SPART_VARR_IS_IRI | SPART_VARR_IS_BLANK | SPART_VARR_IS_LIT) & eq->e_rvr.rvrRestrictions)
    res = res * 3 / 2;
  return res;
}

int
sparp_calc_importance_of_member (sparp_t *sparp, SPART *memb)
{
  int ctr;
  int res = 0;
  switch (memb->type)
    {
    case SPAR_TRIPLE:
      for (ctr = SPART_TRIPLE_FIELDS_COUNT; ctr--; /* no step */)
        {
          SPART *field = memb->_.triple.tr_fields [ctr];
          if (SPAR_IS_BLANK_OR_VAR (field))
            {
              sparp_equiv_t *eq = SPARP_EQUIV (sparp, field->_.var.equiv_idx);
              res += sparp_calc_importance_of_eq (sparp, eq);            
            }
          else
            res += 12;
        }
    case SPAR_GP:
      SPARP_REVFOREACH_GP_EQUIV (sparp, memb, ctr, eq)
        {
          res += sparp_calc_importance_of_eq (sparp, eq);
        }
      END_SPARP_REVFOREACH_GP_EQUIV;
      break;
    }
  return res;
}

int
sparp_find_index_of_most_important_union (sparp_t *sparp, SPART *parent_gp)
{
  int idx, best_idx = -1, best_importance = -1;
  DO_BOX_FAST_REV (SPART *, memb, idx, parent_gp->_.gp.members)
    {
      int importance;
      if ((SPAR_GP != memb->type) || (UNION_L != memb->_.gp.subtype))
        continue;
      importance = sparp_calc_importance_of_member (sparp, memb);
      if (importance >= best_importance) /* '>=', not '>' to give little preference to the leftmost union of a few */
        {
          best_idx = idx;
          best_importance = importance;
        }
    }
  END_DO_BOX_FAST_REV;
  return best_idx; 
}

int
sparp_gp_trav_union_of_joins_in (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  switch (SPART_TYPE(curr))
    {
    case SPAR_GP:
      sts_this[0].sts_env = curr;
      return SPAR_GPT_ENV_PUSH;      
    case SPAR_TRIPLE: return SPAR_GPT_NODOWN |  SPAR_GPT_NOOUT;
    default: return 0;
    }
}

int
sparp_gp_trav_union_of_joins_out (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  if (SPAR_GP != curr->type)
    return 0;
  if ((0 == curr->_.gp.subtype) || (WHERE_L == curr->_.gp.subtype))
    {
      int union_idx;
      int case_ctr, case_count;
      int curr_had_one_member = ((1 >= BOX_ELEMENTS (curr->_.gp.members)) ? 1 : 0);
      SPART *sub_union;
      SPART **detached_join_parts;
      SPART **detached_join_filters;
      SPART **detached_union_parts;
      SPART **detached_union_filters;
      SPART *new_union;
      SPART **new_union_joins;
      if ((curr_had_one_member) && (WHERE_L == curr->_.gp.subtype))
        return 0; /* top-level union can not be really optimized: we need a wrapper to give alias name to retval vars */
      union_idx = sparp_find_index_of_most_important_union (sparp, curr);
      if (0 > union_idx)
        return 0;
      sub_union = curr->_.gp.members [union_idx];
      case_count = BOX_ELEMENTS (sub_union->_.gp.members);
      if (WHERE_L == curr->_.gp.subtype)
        {
          if (curr_had_one_member)
            return 0;
        }
      else
        {
          int parent_len = 1;
          SPART *parent = (SPART *)(sts_this[-1].sts_env);
#ifdef DEBUG
          if (SPAR_GP != SPART_TYPE (parent))
            spar_internal_error (sparp, "sparp_" "gp_trav_union_of_joins_out (): parent is not a gp");
#endif
          if (UNION_L == parent->_.gp.subtype)
            parent_len = BOX_ELEMENTS (parent->_.gp.members);
          if (2000 < (parent_len + case_count))
            return 0; /* This restricts the size of the resulting SQL statement */
        }
      sparp_equiv_audit_all (sparp, 0);
      detached_union_filters = sparp_gp_detach_all_filters (sparp, sub_union, NULL);
      detached_union_parts = sparp_gp_detach_all_members (sparp, sub_union, NULL);
      detached_join_filters = sparp_gp_detach_all_filters (sparp, curr, NULL);
      detached_join_parts = sparp_gp_detach_all_members (sparp, curr, NULL);
      if (curr_had_one_member)
        {
          new_union_joins = detached_union_parts;
        }
      else
        {
          new_union_joins = (SPART **)t_alloc_box (case_count * sizeof (SPART *), DV_ARRAY_OF_POINTER);
          for (case_ctr = 0; case_ctr < case_count; case_ctr++)
            new_union_joins [case_ctr] = sparp_new_empty_gp (sparp, 0, unbox (detached_union_parts [case_ctr]->srcline));
        }
      if (WHERE_L != curr->_.gp.subtype)
        {
          curr->_.gp.subtype = UNION_L;
          new_union = curr;
        }
      else
        {
          new_union = sparp_new_empty_gp (sparp, UNION_L, unbox (curr->srcline));
          sparp_gp_attach_member (sparp, curr, new_union, 0, NULL);
        }
      sparp_gp_attach_many_members (sparp, new_union, new_union_joins, 0, NULL);
      detached_join_parts [union_idx] = NULL;
      for (case_ctr = 0; case_ctr < case_count; case_ctr++)
        {
          int last_case = (((case_count-1) == case_ctr) ? 1 : 0);
          SPART *new_join = new_union->_.gp.members [case_ctr]; /* equal to union_part if curr_had_one_member */
          SPART **new_filts_u = (last_case ? detached_union_filters : sparp_treelist_full_copy (sparp, detached_union_filters, NULL));
          SPART **new_filts_j = (last_case ? detached_join_filters : sparp_treelist_full_copy (sparp, detached_join_filters, NULL));
          SPART **new_join_filts = (SPART **)t_list_concat ((caddr_t)new_filts_u, (caddr_t)new_filts_j);
          if (!curr_had_one_member)
            {
              int join_part_ctr;
              SPART *union_part = detached_union_parts [case_ctr];
              SPART **new_join_parts = (SPART **)t_box_copy ((caddr_t)detached_join_parts);
              new_join_parts [union_idx] = union_part;
              DO_BOX_FAST (SPART *, join_part, join_part_ctr, new_join_parts)
                {
                  if (SPAR_GP == join_part->type)
                    {
                      SPART *cloned_join_part = (last_case ? join_part : sparp_gp_full_clone (sparp, join_part));
                      new_join_parts [join_part_ctr] = cloned_join_part;
                    }
                  else
                    new_join_parts [join_part_ctr] = (last_case ? join_part : sparp_tree_full_copy (sparp, join_part, curr));
                }
              END_DO_BOX_FAST;
              sparp_gp_attach_many_members (sparp, new_join, new_join_parts, 0, NULL);
              sparp_equiv_audit_all (sparp, 0);
            }
          sparp_gp_attach_many_filters (sparp, new_join, new_join_filts, 0, NULL);
          sparp_equiv_audit_all (sparp, 0);
        }
      sparp->sparp_rewrite_dirty += 10;
      sparp_gp_deprecate (sparp, sub_union);
      sparp_equiv_audit_all (sparp, 0);
    }
  return 0;
}

static void
sparp_collect_single_atable_use (sparp_t *sparp, ccaddr_t alias, ccaddr_t tablename, qm_atable_use_t *uses, ptrlong *use_count_ptr )
{
  ptrlong old_qmatu_idx = ecm_find_name (alias, uses, use_count_ptr[0], sizeof (qm_atable_use_t));
      if (ECM_MEM_NOT_FOUND == old_qmatu_idx)
        {
      ptrlong use_idx = ecm_add_name (alias, (void **)(&uses), use_count_ptr, sizeof (qm_atable_use_t));
          qm_atable_use_t *use = uses + use_idx;
      use->qmatu_alias = (char *) alias;
      use->qmatu_tablename = tablename;
          use->qmatu_more = NULL;
        }
      else
        {
          qm_atable_use_t *qmatu = uses + old_qmatu_idx;
      if (strcmp (tablename, qmatu->qmatu_tablename))
            spar_internal_error (sparp, "sparp_" "collect_atable_uses(): probable corruption of some quad map");
        }
}

static void
sparp_collect_atable_uses (sparp_t *sparp, ccaddr_t singletablename, qm_atable_array_t qmatables, qm_atable_use_t *uses, ptrlong *use_count_ptr )
{
  int ata_ctr;
  if ((NULL != singletablename) && ('\0' != singletablename[0]))
    sparp_collect_single_atable_use (sparp, uname___empty, singletablename, uses, use_count_ptr);
  DO_BOX_FAST (qm_atable_t *, ata, ata_ctr, qmatables)
    {
      sparp_collect_single_atable_use (sparp, ata->qmvaAlias, ata->qmvaTableName, uses, use_count_ptr);
    }
  END_DO_BOX_FAST;
}

void
sparp_collect_all_atable_uses (sparp_t *sparp, quad_map_t *qm)
{
  int fld_ctr, max_uses = 0;
  ptrlong use_count = 0;
  qm_atable_use_t *uses;
  for (fld_ctr = 0; fld_ctr < SPART_TRIPLE_FIELDS_COUNT; fld_ctr++)
    {
      qm_value_t *qmv = SPARP_FIELD_QMV_OF_QM (qm, fld_ctr);
      if (NULL != qmv)
        {
          if (NULL != qmv->qmvTableName)
            max_uses++;
        max_uses += BOX_ELEMENTS_0 (qmv->qmvATables);
    }
    }
  if (NULL != qm->qmTableName)
    max_uses++;
  max_uses += BOX_ELEMENTS_0 (qm->qmATables);
  uses = dk_alloc_box_zero (sizeof (qm_atable_use_t) * max_uses, DV_ARRAY_OF_LONG);
  for (fld_ctr = 0; fld_ctr < SPART_TRIPLE_FIELDS_COUNT; fld_ctr++)
    {
      qm_value_t *qmv = SPARP_FIELD_QMV_OF_QM (qm, fld_ctr);
      if (NULL != qmv)
        sparp_collect_atable_uses (sparp, qmv->qmvTableName, qmv->qmvATables, uses, &use_count);
    }
  sparp_collect_atable_uses (sparp, qm->qmTableName, qm->qmATables, uses, &use_count);
  qm->qmAllATableUses = (ptrlong *)uses;
  qm->qmAllATableUseCount = use_count;
}

#if 0
static int
sparp_atable_uses_match (
  qm_atable_array_t qmatables,
  qm_atable_use_t *uses, int use_count )
{
  int ata_ctr, use_ctr, found_ctr = 0;
  DO_BOX_FAST (qm_atable_t *, ata, ata_ctr, qmatables)
    {
      qm_atable_use_t *qmatu;
      for (use_ctr = use_count; use_ctr--; /* no step */)
        {
          qmatu = uses + use_ctr;
          if (strcmp (ata->qmvaAlias, qmatu->qmatu_alias))
            continue;
          if (strcmp (ata->qmvaTableName, qmatu->qmatu_ata->qmvaTableName))
            continue;
          if (NULL == qmatu->qmatu_more)
            {
              found_ctr++;
              qmatu->qmatu_more = ata;
            }
          goto qmatu_exists; /* see below */
        }
      return -1; /* No match because qmatables contains an item that is not in \c uses */
qmatu_exists: ;
    }
  END_DO_BOX_FAST;
  return found_ctr;
}
#endif

static void
sparp_collect_all_conds (sparp_t *sparp, quad_map_t *qm)
{
  int fld_ctr, max_conds = 0;
  ptrlong cond_ctr, cond_count = 0;
  ccaddr_t *conds;
  for (fld_ctr = 0; fld_ctr < SPART_TRIPLE_FIELDS_COUNT; fld_ctr++)
    {
      qm_value_t *qmv = SPARP_FIELD_QMV_OF_QM (qm, fld_ctr);
      if (NULL != qmv)
        max_conds += BOX_ELEMENTS_0 (qmv->qmvConds);
    }
  max_conds += BOX_ELEMENTS_0 (qm->qmConds);
  conds = dk_alloc_box_zero (sizeof (ccaddr_t) * max_conds, DV_ARRAY_OF_LONG);
  for (fld_ctr = 0; fld_ctr < SPART_TRIPLE_FIELDS_COUNT; fld_ctr++)
    {
      qm_value_t *qmv = SPARP_FIELD_QMV_OF_QM (qm, fld_ctr);
      if (NULL == qmv)
        continue;
      DO_BOX_FAST (ccaddr_t, cond, cond_ctr, qmv->qmvConds)
        {
          ecm_map_name (cond, (void **)(&conds), &cond_count, sizeof (ccaddr_t));
        }
      END_DO_BOX_FAST;
    }
  DO_BOX_FAST (ccaddr_t, cond, cond_ctr, qm->qmConds)
    {
      ecm_map_name (cond, (void **)(&conds), &cond_count, sizeof (ccaddr_t));
    }
  END_DO_BOX_FAST;
  qm->qmAllConds = conds;
  qm->qmAllCondCount = cond_count;
}

int
sparp_quad_maps_eq_for_breakup (sparp_t *sparp, quad_map_t *qm_one, quad_map_t *qm_two)
{
  int use_ctr, use_count, cond_ctr, cond_count;
  qm_atable_use_t *uses_one, *uses_two;
  ccaddr_t *conds_one, *conds_two;
/* First of all we check if sets of atables are equal */
  if (NULL == qm_one->qmAllATableUses)
    sparp_collect_all_atable_uses (sparp, qm_one);
  if (NULL == qm_two->qmAllATableUses)
    sparp_collect_all_atable_uses (sparp, qm_two);
  uses_one = (qm_atable_use_t *)(qm_one->qmAllATableUses);
  uses_two = (qm_atable_use_t *)(qm_two->qmAllATableUses);
  use_count = qm_one->qmAllATableUseCount;
  if (use_count != qm_two->qmAllATableUseCount)
    return 0;
  for (use_ctr = use_count; use_ctr--; /* no step */)
    {
      if (strcmp (uses_one[use_ctr].qmatu_alias, uses_two[use_ctr].qmatu_alias))
        return 0;
      if (strcmp (uses_one[use_ctr].qmatu_tablename, uses_two[use_ctr].qmatu_tablename))
        return 0;
    }
  /* If sets of atables are equal then we compare conditions */
  if (NULL == qm_one->qmAllConds)
    sparp_collect_all_conds (sparp, qm_one);
  if (NULL == qm_two->qmAllConds)
    sparp_collect_all_conds (sparp, qm_two);
  conds_one = qm_one->qmAllConds;
  conds_two = qm_two->qmAllConds;
  cond_count = qm_one->qmAllCondCount;
  if (cond_count != qm_two->qmAllCondCount)
    return 0;
  for (cond_ctr = cond_count; cond_ctr--; /* no step */)
    {
      if (strcmp (conds_one[cond_ctr], conds_two[cond_ctr]))
        return 0;
    }
/*return 0;*/
  return 1;
}

caddr_t *
sparp_gp_may_reuse_tabids_in_union (sparp_t *sparp, SPART *gp, int expected_triples_count)
{
  dk_set_t res = NULL;
  int triple_ctr, triples_count;
  if ((SPAR_GP != gp->type) ||
    (0 != gp->_.gp.subtype) )
    return NULL;
  triples_count = BOX_ELEMENTS (gp->_.gp.members);
  if (((0 <= expected_triples_count) && (triples_count != expected_triples_count)) || (0 == triples_count))
    return NULL;
  for (triple_ctr = 0; triple_ctr < triples_count; triple_ctr++)
    {
      SPART * gp_triple = gp->_.gp.members[triple_ctr];
      if (SPAR_TRIPLE != gp_triple->type)
        return NULL;
      if (1 != BOX_ELEMENTS (gp_triple->_.triple.tc_list))
        return NULL;
      if (gp_triple->_.triple.ft_type)
        return NULL; /* TBD: support of free-text indexing in breakup */
    }
  for (triple_ctr = 0; triple_ctr < triples_count; triple_ctr++)
    {
      SPART * gp_triple = gp->_.gp.members[triple_ctr];
      t_set_push (&res, gp_triple->_.triple.tabid);
    }
  return t_revlist_to_array (res);
}

int
sparp_try_reuse_tabid_in_union (sparp_t *sparp, SPART *curr, int base_idx)
{
  SPART *base = curr->_.gp.members[base_idx];
  /*SPART **base_filters = base->_.gp.filters; !!!TBD: check for fitlers that may restrict the search by idex */
  SPART **base_triples = base->_.gp.members;
  int bt_ctr, base_triples_count = BOX_ELEMENTS (base_triples);
  int dep_idx, memb_count, breakup_shift = 0, breakup_unictr = -1;
  int base_should_change_tabid = 0;
  memb_count = BOX_ELEMENTS_0 (curr->_.gp.members);
  for (dep_idx = base_idx + 1; /* breakup optimization is symmetrical so the case of two triples should be considered only once, not base_triple...dep then dep...base_triple */
    dep_idx < memb_count; dep_idx++)
    {
      SPART *dep = curr->_.gp.members[dep_idx];
      SPART **dep_triples;
      if (NULL == sparp_gp_may_reuse_tabids_in_union (sparp, dep, base_triples_count))
        continue;
      if (dep_idx == base_idx + 1)
        base_should_change_tabid = 1; /* There's a danger of tabid collision so printer will treat base and dep as breakup even if it is not true */
      dep_triples = dep->_.gp.members;
      if (BOX_ELEMENTS (dep_triples) != base_triples_count)
        goto next_dep; /* see below */        
      for (bt_ctr = base_triples_count; bt_ctr--; /* no step */)
        {
          SPART *base_triple = base_triples[bt_ctr];
          SPART *dep_triple = dep_triples[bt_ctr];
          quad_map_t *base_qm, *dep_qm;
          int fld_ctr;
          if (dep_triple->_.triple.src_serial != base_triple->_.triple.src_serial)
            goto next_dep; /* see below */
          base_qm = base_triple->_.triple.tc_list[0]->tc_qm;
          dep_qm = dep_triple->_.triple.tc_list[0]->tc_qm;
          if (!sparp_expn_lists_are_equal (sparp, base->_.gp.filters, dep->_.gp.filters))
            goto next_dep; /* see below */
          if (!sparp_quad_maps_eq_for_breakup (sparp, base_qm, dep_qm))
            goto next_dep; /* see below */
          for (fld_ctr = 0; fld_ctr < SPART_TRIPLE_FIELDS_COUNT; fld_ctr++)
            { /* For each variable that makes equalities we check that it has identical qmv in base and in dep */
              SPART *base_fld = base_triple->_.triple.tr_fields[fld_ctr];
              SPART *dep_fld = dep_triple->_.triple.tr_fields[fld_ctr];
              sparp_equiv_t *base_fld_eq, *dep_fld_eq;
              long base_restr, dep_restr;
              int fld_has_equations;
              qm_value_t *base_fld_qmv, *dep_fld_qmv;
              if (!SPAR_IS_BLANK_OR_VAR (base_fld))
                continue;
              if (!SPAR_IS_BLANK_OR_VAR (dep_fld))
                spar_internal_error (sparp, "sparp_" "try_reuse_tabid_in_union(): different field types in triples with same src_serial");
              base_fld_eq = SPARP_EQUIV (sparp, base_fld->_.var.equiv_idx);
              dep_fld_eq = SPARP_EQUIV (sparp, dep_fld->_.var.equiv_idx);
              base_fld_qmv = SPARP_FIELD_QMV_OF_QM (base_qm, fld_ctr);
              dep_fld_qmv = SPARP_FIELD_QMV_OF_QM (dep_qm, fld_ctr);
              base_restr = base_fld_eq->e_rvr.rvrRestrictions;
              dep_restr = dep_fld_eq->e_rvr.rvrRestrictions;
              fld_has_equations = ((1 < base_fld_eq->e_var_count) ||
                (base_restr & (SPART_VARR_GLOBAL | SPART_VARR_EXTERNAL)) ||
                ((base_restr & SPART_VARR_FIXED) && (NULL != base_fld_qmv)) ||
                ((dep_restr & SPART_VARR_FIXED) && (NULL != dep_fld_qmv)) );
              if (!fld_has_equations)
                continue;
              if (base_fld_qmv != dep_fld_qmv)
                goto next_dep; /* see below */
              if (NULL == base_fld_qmv)
                {
                  caddr_t base_fld_const = SPARP_FIELD_CONST_OF_QM (base_qm, fld_ctr);
                  caddr_t dep_fld_const = SPARP_FIELD_CONST_OF_QM (dep_qm, fld_ctr);
                  if (((base_restr & SPART_VARR_IS_REF) && (dep_restr & SPART_VARR_IS_LIT)) ||
                    ((base_restr & SPART_VARR_IS_LIT) && (dep_restr & SPART_VARR_IS_REF)) )
                    goto next_dep; /* see below */
                  if (!sparp_fixedvalues_equal (sparp, base_fld_const, dep_fld_const))
                    goto next_dep; /* see below */
                }
            }
        }
      /* At this point all checks of dep are passed, can adjust selids and tabids */
      sparp_equiv_audit_all (sparp, SPARP_EQUIV_AUDIT_NOBAD);
      if (0 == breakup_shift)
        breakup_unictr = sparp->sparp_unictr++;
      for (bt_ctr = base_triples_count; bt_ctr--; /* no step */)
        {
          SPART *base_triple = base_triples[bt_ctr];
          SPART *dep_triple = dep_triples[bt_ctr];
          caddr_t new_base_tabid;
          if (0 == breakup_shift)
            {
              new_base_tabid = t_box_sprintf (200, "%s-b%d", base_triple->_.triple.tabid, breakup_unictr);
              sparp_set_triple_selid_and_tabid (sparp, base_triple, base->_.gp.selid, new_base_tabid);
              base_should_change_tabid = 0;
        }
          else
            new_base_tabid = base_triple->_.triple.tabid;
          sparp_set_triple_selid_and_tabid (sparp, dep_triple, dep->_.gp.selid, new_base_tabid);
        }
      if (dep_idx > (base_idx + 1 + breakup_shift)) /* Adjustment to keep reused tabids together. */
        {
          int swap_ctr;
          for (swap_ctr = dep_idx; swap_ctr > (base_idx + 1 + breakup_shift); swap_ctr--)
            curr->_.gp.members[swap_ctr] = curr->_.gp.members[swap_ctr-1];
          curr->_.gp.members[base_idx + 1 + breakup_shift] = dep;
        }
      breakup_shift++;
      sparp_equiv_audit_all (sparp, SPARP_EQUIV_AUDIT_NOBAD);

next_dep: ;
    }
#ifdef DEBUG
  if (0 != breakup_shift)
    {
      int chk_idx;
      printf ("sparp_" "try_reuse_tabid_in_union() has found breakup in %s from %d to %d incl.\n",
        curr->_.gp.selid, base_idx, base_idx + breakup_shift);
      for (chk_idx = base_idx + breakup_shift; chk_idx > base_idx; chk_idx--)
        {
          SPART *chk = curr->_.gp.members[chk_idx];
          for (bt_ctr = base_triples_count; bt_ctr--; /* no step */)
            {
              SPART *base_triple = base_triples[bt_ctr];
              SPART *chk_triple = chk->_.gp.members[bt_ctr];
              if (!sparp_quad_maps_eq_for_breakup (sparp, base_triple->_.triple.tc_list[0]->tc_qm, chk_triple->_.triple.tc_list[0]->tc_qm))
                spar_internal_error (sparp, "sparp_" "try_reuse_tabid_in_union() has made breakup on inappropriate pair");
            }
        }
    }
#endif
  if (base_should_change_tabid)
    { /* We rename tabids in base to prevent occasional recognition of breakup in printer */
      breakup_unictr = sparp->sparp_unictr++;
      for (bt_ctr = base_triples_count; bt_ctr--; /* no step */)
        {
          SPART *base_triple = base_triples[bt_ctr];
          if (0 == breakup_shift)
            {
              caddr_t new_base_tabid;
              new_base_tabid = t_box_sprintf (200, "%s-u%d", base_triple->_.triple.tabid, breakup_unictr);
              sparp_set_triple_selid_and_tabid (sparp, base_triple, base->_.gp.selid, new_base_tabid);
            }
        }
    }
  return breakup_shift;
}



static int
sparp_qmv_forms_reusable_key_of_qm (sparp_t *sparp, qm_value_t *key_qmv, quad_map_t *qm)
{
  dk_set_t key_aliases = NULL;
  int ctr, fld_ctr;
  if (NULL == key_qmv) /* Funny case: join on field that is mapped to constant. Thus there's no actual join, only filter (FIELD = CONST) for other triples */
    return 0;
  if (!key_qmv->qmvColumnsFormKey) /* No key -- no reuse */
    return 0;
  if (!key_qmv->qmvFormat->qmfIsBijection)	/* Non-bijection format is unsafe for reuse, different SHORTs may result is same LONG */
    return 0;
  DO_BOX_FAST (qm_atable_t *, at, ctr, key_qmv->qmvATables)
    {
      if (0 > dk_set_position_of_string (key_aliases, at->qmvaAlias))
        t_set_push (&key_aliases, (caddr_t)(at->qmvaAlias));
    }
  END_DO_BOX_FAST;
  DO_BOX_FAST (qm_atable_t *, at, ctr, qm->qmATables)
    { /* If qm uses aliases not 'selected' by key_qmv field then the key value of key_qvm is not a key of whole triple */
      if (0 > dk_set_position_of_string (key_aliases, at->qmvaAlias))
        return 0;
    }
  END_DO_BOX_FAST;
  for (fld_ctr = 0; fld_ctr < SPART_TRIPLE_FIELDS_COUNT; fld_ctr++)
    { /* If any field of qm uses aliases not 'selected' by key_qmv field then the key value of key_qvm is not a key of whole triple */
      qm_value_t *fld_qmv = SPARP_FIELD_QMV_OF_QM (qm, fld_ctr);
      if (fld_qmv == key_qmv)
        continue; /* No check, key_qmv is a key for itself */
      if (NULL == fld_qmv)
        continue;
      DO_BOX_FAST (qm_atable_t *, at, ctr, fld_qmv->qmvATables)
        {
          if (0 > dk_set_position_of_string (key_aliases, at->qmvaAlias))
            return 0;
        }
      END_DO_BOX_FAST;
    }
  return 1;
}


int
sparp_try_reuse_tabid_in_join (sparp_t *sparp, SPART *curr, int base_idx)
{
  SPART *base = (SPART *)(curr->_.gp.members[base_idx]);
  quad_map_t *base_qm = base->_.triple.tc_list[0]->tc_qm;
  int key_field_idx, join_offset = 0;
          for (key_field_idx = 0; key_field_idx < SPART_TRIPLE_FIELDS_COUNT; key_field_idx++)
            {
              SPART *key_field = base->_.triple.tr_fields[key_field_idx];
              ssg_valmode_t key_fmt = base->_.triple.native_formats[key_field_idx];
              qm_value_t *key_qmv;
              sparp_equiv_t *key_eq;
              int dep_ctr;
              sparp_jso_validate_format (sparp, key_fmt);
              if (!SPAR_IS_BLANK_OR_VAR (key_field)) /* Non-variables can not result in tabid reuse atm, !!!TBD: support for { <pk> ?p1 ?o1 . <pk> ?p2 ?o2 } */
                continue;
              key_qmv = SPARP_FIELD_QMV_OF_QM (base_qm,key_field_idx);
              if (!sparp_qmv_forms_reusable_key_of_qm (sparp, key_qmv, base_qm))
                continue;
              key_eq = sparp_equiv_get (sparp, curr, key_field, SPARP_EQUIV_GET_ASSERT);
              if (2 > key_eq->e_gspo_uses) /* No reuse of key -- no reuse of triples */
                continue;
              for (dep_ctr = key_eq->e_var_count; dep_ctr--; /* no step */)
                {
                  SPART *dep_field = key_eq->e_vars[dep_ctr];
                  int dep_triple_idx, dep_field_tr_idx;
          SPART *dep_triple = NULL;
                  quad_map_t *dep_qm;
                  qm_value_t *dep_qmv;
                  if (NULL == dep_field->_.var.tabid) /* The variable is not a field in a triple (const read, not gspo use) */
                    continue;
                  if (!strcmp (dep_field->_.var.tabid, key_field->_.var.tabid)) /* Either tabid is reused already or reference to self -- nothing to do in both cases */
                    continue;
                  dep_field_tr_idx = dep_field->_.var.tr_idx;
                  for (dep_triple_idx = BOX_ELEMENTS (curr->_.gp.members); dep_triple_idx--; /* no step */)
                    {
                      dep_triple = curr->_.gp.members[dep_triple_idx];
                      if (SPAR_TRIPLE != dep_triple->type)
                        continue;
                      if (dep_triple->_.triple.tr_fields [dep_field_tr_idx] == dep_field)
                        break;
                    }
                  if (0 > dep_triple_idx)
            {
              sparp_equiv_audit_all (sparp, SPARP_EQUIV_AUDIT_NOBAD);
                    spar_internal_error (sparp, "sparp_" "gp_trav_reuse_tabids(): dep_field not found in members");
            }
          if (dep_triple_idx <= (base_idx + join_offset)) /* Merge is symmetrical, so this pair of key and dep is checked from other end. In that time current dep was base and the current base was dep */
                    continue;
                  if (1 != BOX_ELEMENTS (dep_triple->_.triple.tc_list)) /* Only triples with one allowed quad mapping can be reused, unions can not */
                    continue;
                  dep_qm = dep_triple->_.triple.tc_list[0]->tc_qm;
          #if 0 /* There's no need to check this because if QMVs match then tables are the same, otherwise names does not matter anyway */
                  if (strcmp (dep_qm->qmTableName, base_qm->qmTableName)) /* Can not reuse tabid for different tables */
                    continue;
          #endif
                  dep_qmv = SPARP_FIELD_QMV_OF_QM (dep_qm, dep_field_tr_idx);
                  if (key_qmv != dep_qmv) /* The key mapping differs in set of source columns or in the IRI serialization (or literal cast) */
                    continue;
                  if (!sparp_qmv_forms_reusable_key_of_qm (sparp, dep_qmv, dep_qm))
                    continue;
                  /* Glory, glory, hallelujah; we can reuse the tabid so the final SQL query will have one join less. */
          sparp_equiv_audit_all (sparp, SPARP_EQUIV_AUDIT_NOBAD);
                  sparp_set_triple_selid_and_tabid (sparp, dep_triple, curr->_.gp.selid, base->_.triple.tabid);
          if (dep_triple_idx > (base_idx + join_offset + 1)) /* Adjustment to keep reused tabids together. The old join order of dep is of zero importance because there's no more dep as a separate subtable */
                    {
                      int swap_ctr;
              for (swap_ctr = dep_triple_idx; swap_ctr > (base_idx + join_offset + 1); swap_ctr--)
                curr->_.gp.members[swap_ctr] = curr->_.gp.members[swap_ctr-1];
              curr->_.gp.members[base_idx + join_offset + 1] = dep_triple;
                    }
          sparp_equiv_audit_all (sparp, SPARP_EQUIV_AUDIT_NOBAD);
                }
            }
  return join_offset;
}


int
sparp_gp_trav_reuse_tabids (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  int base_idx;
  if (SPAR_GP != curr->type)
    return 0;
  switch (curr->_.gp.subtype)
    {
    case UNION_L:
      DO_BOX_FAST (SPART *, base, base_idx, curr->_.gp.members)
        {
          int breakup_offset;
          if (NULL == sparp_gp_may_reuse_tabids_in_union (sparp, base, -1))
            continue;
          breakup_offset = sparp_try_reuse_tabid_in_union (sparp, curr, base_idx);
          base_idx += breakup_offset;
        }
      END_DO_BOX_FAST;
      break;
    case 0: case WHERE_L:
      DO_BOX_FAST (SPART *, base, base_idx, curr->_.gp.members)
        {
          int join_offset;
          if (SPAR_TRIPLE != base->type) /* Only triples have tabids to merge */
            continue;
          if (1 != BOX_ELEMENTS (base->_.triple.tc_list)) /* Only triples with one allowed quad map can be reused, unions can not */
            continue;
          join_offset = sparp_try_reuse_tabid_in_join (sparp, curr, base_idx);
          base_idx += join_offset;
        }
      END_DO_BOX_FAST;
      break;
    }
  return 0;
}


void
sparp_rewrite_all (sparp_t *sparp, int safely_copy_retvals)
{
  if (SPAR_QM_SQL_FUNCALL == SPART_TYPE (sparp->sparp_expr))
    return;
  sparp_rewrite_retvals (sparp, safely_copy_retvals);
  if (sparp->sparp_env->spare_grab.rgc_pview_mode)
    {
      sparp_rewrite_grab (sparp);
      return;
    }
  sparp_rewrite_qm (sparp);
}


void
sparp_rewrite_qm (sparp_t *sparp)
{
  if (SPAR_CODEGEN == SPART_TYPE (sparp->sparp_expr))
    return;
  if (SPAR_QM_SQL_FUNCALL == SPART_TYPE (sparp->sparp_expr))
    return;
  sparp_rewrite_qm_preopt (sparp, 1);
  sparp_rewrite_qm_optloop (sparp, SPARP_MULTIPLE_OPTLOOPS);
  sparp_rewrite_qm_postopt (sparp);
}

void
sparp_rewrite_qm_preopt (sparp_t *sparp, int safely_copy_retvals)
{
  sparp_equiv_t **equivs /* do not set here */;
  int equiv_ctr, equiv_count;
  if (SPAR_CODEGEN == SPART_TYPE (sparp->sparp_expr))
    GPF_T1 ("sparp_" "rewrite_qm_postopt () for CODEGEN");
  if (SPAR_QM_SQL_FUNCALL == SPART_TYPE (sparp->sparp_expr))
    GPF_T1 ("sparp_" "rewrite_qm_preopt () for SQL_FUNCALL");
  sparp_rewrite_basic (sparp);
  equivs = sparp->sparp_equivs;
  equiv_count = sparp->sparp_equiv_count;
  for (equiv_ctr = equiv_count; equiv_ctr--; /* no step */)
    {
      sparp_equiv_t *eq = equivs[equiv_ctr];
      if (NULL == eq)
        continue;
      if (
        !(eq->e_rvr.rvrRestrictions & (SPART_VARR_FIXED | SPART_VARR_GLOBAL | SPART_VARR_EXTERNAL)) &&
        (0 == eq->e_gspo_uses) && (0 == eq->e_subquery_uses) &&
        (0 == eq->e_nested_bindings) && (0 == BOX_ELEMENTS_0 (eq->e_subvalue_idxs)) &&         
	((0 != eq->e_const_reads) || /* note: no check for (0 != eq->e_optional_reads) */
          (0 != BOX_ELEMENTS_0 (eq->e_receiver_idxs)) ) )
        {
          if (eq->e_rvr.rvrRestrictions & SPART_VARR_EXPORTED)
            spar_error (sparp, "Variable '%.100s' is used in the query result set but not assigned", eq->e_varnames[0]);
          if ((0 != eq->e_const_reads) || /* note: no check for (0 != eq->e_optional_reads) */
            (0 != BOX_ELEMENTS_0 (eq->e_receiver_idxs)) )
            {
            spar_error (sparp, "Variable '%.100s' is used in subexpressions of the query but not assigned", eq->e_varnames[0]);
        }
    }
    }
  sparp->sparp_storage = sparp_find_storage_by_name (sparp->sparp_expr->_.req_top.storage_name);
/* Building qm_list for every triple in the tree. */
  sparp_gp_trav (sparp, sparp->sparp_expr->_.req_top.pattern, NULL,
    sparp_gp_trav_refresh_triple_cases, NULL,
    NULL, NULL, NULL,
    NULL );
  sparp_equiv_audit_all (sparp, 0);
  sparp_rewrite_basic (sparp);
}

int
sparp_gp_trav_rewrite_qm_optloop (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  sparp_t *sub_sparp = sparp_down_to_sub (sparp, curr);
  if (sparp_rewrite_qm_optloop (sub_sparp, (ptrlong)common_env))
    sparp->sparp_rewrite_dirty = 1;
  sparp_up_from_sub (sparp, curr, sub_sparp);
  return 0;
}

int
sparp_rewrite_qm_optloop (sparp_t *sparp, int opt_ctr)
{
  int res;
  int saved_sparp_rewrite_dirty = sparp->sparp_rewrite_dirty;
  if (SPAR_CODEGEN == SPART_TYPE (sparp->sparp_expr))
    GPF_T1 ("sparp_" "rewrite_qm_postopt () for CODEGEN");
  if (SPAR_QM_SQL_FUNCALL == SPART_TYPE (sparp->sparp_expr))
    GPF_T1 ("sparp_" "rewrite_qm_optloop () for SQL_FUNCALL");
  if (SPARP_MULTIPLE_OPTLOOPS == opt_ctr)
    {
      int opt_ctr = 0;
      while (sparp_rewrite_qm_optloop (sparp, opt_ctr))
        {
          if (opt_ctr++ < 10)
            continue;
          spar_internal_error (sparp, "SPARQL optimizer performed 10 rounds of query rewriting, this looks like endless loop. Please rephrase the query.");
        }
      return opt_ctr;
    }
  sparp_equiv_audit_all (sparp, 0);
  sparp->sparp_rewrite_dirty = 0;
/* Converting to GP_UNION of every triple such that many quad maps contains triples that matches the mapping pattern */
  sparp_gp_trav (sparp, sparp->sparp_expr->_.req_top.pattern, (void *)((ptrlong)opt_ctr),
    sparp_gp_trav_refresh_triple_cases, sparp_gp_trav_multiqm_to_unions,
    NULL, NULL, sparp_gp_trav_rewrite_qm_optloop,
    NULL );
  sparp_equiv_audit_all (sparp, 0);
  sparp_rewrite_basic (sparp);
  sparp_equiv_audit_all (sparp, SPARP_EQUIV_AUDIT_NOBAD);
/* Converting join with a union into a union of joins with parts of union */
  sparp_gp_trav (sparp, sparp->sparp_expr->_.req_top.pattern, NULL,
    sparp_gp_trav_union_of_joins_in, sparp_gp_trav_union_of_joins_out,
    NULL, NULL, NULL,
    NULL );
  sparp_equiv_audit_all (sparp, 0);
  sparp_rewrite_basic (sparp);
/* Removal of gps that can not produce results */
  sparp_gp_trav (sparp, sparp->sparp_expr->_.req_top.pattern, NULL,
    NULL, sparp_gp_trav_detach_conflicts_out,
    NULL, NULL, NULL,
    NULL );
  sparp_equiv_audit_all (sparp, 0);
  sparp_rewrite_basic (sparp);
  sparp_gp_trav (sparp, sparp->sparp_expr->_.req_top.pattern, NULL,
    NULL, sparp_gp_trav_detach_conflicts_out,
    NULL, NULL, NULL,
    NULL );
  sparp_equiv_audit_all (sparp, 0);
  sparp_rewrite_basic (sparp);
  sparp_equiv_audit_all (sparp, 0);
  if (!(opt_ctr % 2)) /* Do this not in every loop because it's costly and it almost never give a result after first loop */
    {
      sparp_gp_trav (sparp, sparp->sparp_expr->_.req_top.pattern, NULL,
        sparp_gp_trav_localize_filters, NULL,
        NULL, NULL, NULL,
        NULL );
      sparp_equiv_audit_all (sparp, 0);
      sparp_rewrite_basic (sparp);
      sparp_equiv_audit_all (sparp, 0);
    }
  sparp_equiv_audit_all (sparp, SPARP_EQUIV_AUDIT_NOBAD);
  sparp_trav_out_clauses (sparp, sparp->sparp_expr, sparp->sparp_expr->_.req_top.pattern,
    NULL, NULL,
    NULL, NULL, sparp_gp_trav_rewrite_qm_optloop,
    NULL );
  sparp_equiv_audit_all (sparp, SPARP_EQUIV_AUDIT_NOBAD);
  res = sparp->sparp_rewrite_dirty;
  sparp->sparp_rewrite_dirty = saved_sparp_rewrite_dirty;
  return res;
}

int
sparp_gp_trav_rewrite_qm_postopt (sparp_t *sparp, SPART *curr, sparp_trav_state_t *sts_this, void *common_env)
{
  sparp_t *sub_sparp = sparp_down_to_sub (sparp, curr);
  sparp_rewrite_qm_postopt (sub_sparp);
  sparp_up_from_sub (sparp, curr, sub_sparp);
  return 0;
}

void
sparp_rewrite_qm_postopt (sparp_t *sparp)
{
  sparp_equiv_t **equivs;
  int equiv_ctr, equiv_count;
  int retval_ctr;
  SPART *root = sparp->sparp_expr;
  if (SPAR_CODEGEN == SPART_TYPE (sparp->sparp_expr))
    GPF_T1 ("sparp_" "rewrite_qm_postopt () for CODEGEN");
  if (SPAR_QM_SQL_FUNCALL == SPART_TYPE (sparp->sparp_expr))
    GPF_T1 ("sparp_" "rewrite_qm_postopt () for SQL_FUNCALL");
  sparp_wpar_retvars_in_max (sparp, sparp->sparp_expr);
  sparp_gp_trav (sparp, sparp->sparp_expr->_.req_top.pattern, NULL,
    NULL, sparp_gp_trav_reuse_tabids,
    NULL, NULL, sparp_gp_trav_rewrite_qm_postopt,
    NULL );
  sparp_trav_out_clauses (sparp, sparp->sparp_expr, NULL,
    NULL, NULL,
    NULL, NULL, sparp_gp_trav_rewrite_qm_postopt,
    NULL );
/* Final processing: */
  switch (root->_.req_top.subtype)
    {
    case INSERT_L: case DELETE_L:
      spar_optimize_retvals_of_insert_or_delete (sparp, root);
      break;
    case MODIFY_L:
      spar_optimize_retvals_of_modify (sparp, root);
      break;
    }
  if (NULL != sparp->sparp_parent_sparp)
    goto end_of_equiv_checks; /* see below */
  equivs = sparp->sparp_equivs;
  equiv_count = sparp->sparp_equiv_count;
  for (equiv_ctr = equiv_count; equiv_ctr--; /* no step */)
    {
      sparp_equiv_t *eq = equivs[equiv_ctr];
      if (NULL == eq)
        continue;
      if (eq->e_deprecated)
        continue;
      if ((0 == eq->e_gspo_uses) && (0 == eq->e_subquery_uses) &&
        (0 == eq->e_nested_bindings) &&
	((0 != eq->e_const_reads) || /* note: no check for (0 != eq->e_optional_reads) */
          (0 != BOX_ELEMENTS_0 (eq->e_receiver_idxs)) ) &&
        !(eq->e_rvr.rvrRestrictions & (SPART_VARR_FIXED | SPART_VARR_GLOBAL | SPART_VARR_EXTERNAL)) )
        {
          if (!sparp->sparp_env->spare_signal_void_variables)
            eq->e_rvr.rvrRestrictions |= SPART_VARR_CONFLICT;
          else if (eq->e_rvr.rvrRestrictions & SPART_VARR_EXPORTED)
            spar_error (sparp, "Variable '%.100s' can not be bound due to mutially exclusive restrictions on its value", eq->e_varnames[0]);
          else if ((0 != eq->e_const_reads) || /* note: no check for (0 != eq->e_optional_reads) */
            (0 != BOX_ELEMENTS_0 (eq->e_receiver_idxs)) )
            spar_error (sparp, "Variable '%.100s' is used in subexpressions of the query but can not be assigned", eq->e_varnames[0]);
        }
#ifdef DEBUG
      equivs[equiv_ctr]->e_dbg_saved_gp = (SPART **)t_box_num ((ptrlong)(equivs[equiv_ctr]->e_gp));
#endif
/*
      equivs[equiv_ctr]->e_gp = NULL;
*/
    }

end_of_equiv_checks:
  DO_BOX_FAST (SPART *, retval, retval_ctr, root->_.req_top.retvals)
    {
      caddr_t name;
      int retval_type = SPART_TYPE (retval);
      if (SPAR_VARIABLE == retval_type)
        name = retval->_.var.vname;
      else if (SPAR_ALIAS == retval_type)
        name = retval->_.alias.aname;
      else
        continue;
      if (!strchr (name, '>'))
        continue;
      DO_SET (spar_propvariable_t *, pv, &(sparp->sparp_propvars))
        {
          const char *optext;
          if (strcmp (pv->sparpv_obj_var_name, name))
            continue;
          optext = ((_PLUS_GT == pv->sparpv_op) ? "+>" : "*>");
          if (pv->sparpv_obj_altered)
            spar_error (sparp, "The name of a result-set column for variable ?%.200s %s %s?%.200s%s %s; please specify some alias.",
              pv->sparpv_subj_var->_.var.vname, optext,
              ((Q_IRI_REF == pv->sparpv_verb_lexem_type) ? "<" : ""), pv->sparpv_verb_lexem_text,
              ((Q_IRI_REF == pv->sparpv_verb_lexem_type) ? ">" : ""),
              ((pv->sparpv_obj_altered & 0x2) ? "may cause subtle coding errors" : "is too long") );
          break;
        }
      END_DO_SET();
    }
  END_DO_BOX_FAST;
}


void
sparp_rewrite_grab (sparp_t *sparp)
{
  sparp_env_t *env = sparp->sparp_env;
  rdf_grab_config_t *rgc = &(env->spare_grab);
  sparp_t *sparp_of_seed;	/* This will compile the statement that will collect the first set of graphs */
  sparp_t *sparp_of_iter;	/* This will compile the statement that will called while the set of graphs growth */
  sparp_t *sparp_of_final;	/* This will compile the statement that will produce the final result set */
  sparp_t *sub_sparps[3];
  caddr_t sql_texts[3];
  SPART **grab_retvals;
  caddr_t retselid;
  ptrlong top_subtype;
  dk_set_t new_vars = NULL;
  dk_set_t sa_graphs = NULL;
  spar_sqlgen_t ssg;
  sql_comp_t sc;
  int sub_sparp_ctr;
  ptrlong rgc_flags = 0;
  int use_plain_return;
  retselid = sparp->sparp_expr->_.req_top.retselid;
  top_subtype = sparp->sparp_expr->_.req_top.subtype;
  use_plain_return = (((CONSTRUCT_L == top_subtype) || (DESCRIBE_L == top_subtype)) ? 1 : 0);
  t_set_push (&(env->spare_selids), retselid);
  DO_SET (caddr_t, grab_name, &(rgc->rgc_vars))
    {
      t_set_push (&new_vars, spar_make_variable (sparp, grab_name));
    }
  END_DO_SET()
  if (NULL != rgc->rgc_destination)
    t_set_push (&sa_graphs, spar_make_qm_sql (sparp, "iri_to_id", (SPART **)t_list (1, rgc->rgc_destination), NULL));
  if (NULL != rgc->rgc_group_destination)
    t_set_push (&sa_graphs, spar_make_qm_sql (sparp, "iri_to_id", (SPART **)t_list (1, rgc->rgc_group_destination), NULL));
  t_set_pop (&(env->spare_selids));
  if (NULL != new_vars)
  grab_retvals = (SPART **)t_revlist_to_array (new_vars);
  else
    grab_retvals = sparp_treelist_full_copy (sparp, sparp->sparp_expr->_.req_top.expanded_orig_retvals, NULL);
/* Making subqueries: seed */
  sub_sparps[0] = sparp_of_seed = sparp_clone_for_variant (sparp);
  sparp_of_seed->sparp_expr = sparp_tree_full_copy (sparp_of_seed, sparp->sparp_expr, NULL);
  sparp_of_seed->sparp_expr->_.req_top.shared_spare = sparp_of_seed->sparp_env;
  sparp_of_seed->sparp_expr->_.req_top.subtype = SELECT_L;
  sparp_of_seed->sparp_expr->_.req_top.retvals = grab_retvals;
  sparp_of_seed->sparp_expr->_.req_top.retvalmode_name = t_box_string ("LONG");
  sparp_of_seed->sparp_expr->_.req_top.limit = t_box_num (SPARP_MAXLIMIT);
  sparp_of_seed->sparp_expr->_.req_top.offset = 0;
  sparp_of_seed->sparp_env->spare_globals_are_numbered = 1;
  sparp_of_seed->sparp_env->spare_global_num_offset = 1;
  sparp_of_seed->sparp_env->spare_grab.rgc_sa_graphs = env->spare_grab.rgc_sa_graphs;
  sparp_of_seed->sparp_env->spare_grab.rgc_sa_preds = env->spare_grab.rgc_sa_preds;
  sparp_of_seed->sparp_env->spare_grab.rgc_sa_vars = env->spare_grab.rgc_sa_vars;
  sparp_of_seed->sparp_env->spare_grab.rgc_vars = env->spare_grab.rgc_vars;
/* Making subqueries: iter */
  sub_sparps[1] = sparp_of_iter = sparp_clone_for_variant (sparp_of_seed);
  sparp_of_iter->sparp_expr = sparp_tree_full_copy (sparp_of_seed, sparp_of_seed->sparp_expr, NULL);
  sparp_of_iter->sparp_expr->_.req_top.shared_spare = sparp_of_iter->sparp_env;
  sparp_of_iter->sparp_env->spare_globals_are_numbered = 1;
  sparp_of_iter->sparp_env->spare_global_num_offset = 1;
  sparp_of_iter->sparp_env->spare_grab.rgc_sa_graphs = env->spare_grab.rgc_sa_graphs;
  sparp_of_iter->sparp_env->spare_grab.rgc_sa_preds = env->spare_grab.rgc_sa_preds;
  sparp_of_iter->sparp_env->spare_grab.rgc_sa_vars = env->spare_grab.rgc_sa_vars;
  sparp_of_iter->sparp_env->spare_grab.rgc_vars = env->spare_grab.rgc_vars;
/*!!! TBD: relax graph conditions in sparp_of_iter */
/* Making subqueries: final */
  sub_sparps[2] = sparp_of_final = sparp_clone_for_variant (sparp);
  sparp_of_final->sparp_expr = sparp_tree_full_copy (sparp_of_seed, sparp->sparp_expr, NULL);
  sparp_of_final->sparp_expr->_.req_top.shared_spare = sparp_of_final->sparp_env;
  sparp_of_final->sparp_env->spare_globals_are_numbered = 1;
  sparp_of_final->sparp_env->spare_global_num_offset = 0;
/*!!! TBD: relax graph conditions in sparp_of_final */
  for (sub_sparp_ctr = 3; sub_sparp_ctr--; /* no step */)
    {
      sparp_t *sub_sparp = sub_sparps [sub_sparp_ctr];
      sparp_rewrite_qm (sub_sparp);
      memset (&ssg, 0, sizeof (spar_sqlgen_t));
      memset (&sc, 0, sizeof (sql_comp_t));
      sc.sc_client = sub_sparp->sparp_sparqre->sparqre_cli;
      ssg.ssg_out = strses_allocate ();
      ssg.ssg_sc = &sc;
      ssg.ssg_sparp = sub_sparp;
      ssg.ssg_tree = sub_sparp->sparp_expr;
      ssg.ssg_sources = ssg.ssg_tree->_.req_top.sources; /*!!!TBD merge with environment */
      ssg_make_sql_query_text (&ssg);
      sql_texts [sub_sparp_ctr] = t_strses_string (ssg.ssg_out);
      strses_free (ssg.ssg_out);
    }
  if (rgc->rgc_intermediate)
    rgc_flags |= 0x0001;
  sparp->sparp_expr = spartlist (sparp, 21, SPAR_CODEGEN, /* #0 */
    t_box_num ((ptrlong)(ssg_grabber_codegen)),
    sparp_treelist_full_copy (sparp, sparp->sparp_expr->_.req_top.retvals, NULL),	/* #2 */
    t_box_dv_short_string ("sql:RDF_GRAB"),	/* #3 */
    sql_texts[0], sql_texts[1], sql_texts[2], /* #4-#6 */
    t_box_copy (sparp->sparp_expr->_.req_top.limit),	/* #7 */
    ((NULL == rgc->rgc_consts) ? NULL :
      spar_make_vector_qm_sql (sparp, (SPART **)(t_revlist_to_array (rgc->rgc_consts))) ), /* #8 */
    ((NULL == sa_graphs) ? NULL :
      spar_make_vector_qm_sql (sparp, (SPART **)(t_revlist_to_array (sa_graphs))) ), /* #9 */
    ((NULL == rgc->rgc_sa_preds) ? NULL :
      spar_make_vector_qm_sql (sparp, (SPART **)(t_revlist_to_array (rgc->rgc_sa_preds))) ), /* #10 */
    t_box_copy (rgc->rgc_depth), t_box_copy (rgc->rgc_limit), /* #11-#12 */
    t_box_copy (rgc->rgc_base),	/* #13 */
    t_box_copy (rgc->rgc_destination), t_box_copy (rgc->rgc_group_destination),	/* #14-#15 */
    t_box_copy (rgc->rgc_resolver_name), t_box_copy (rgc->rgc_loader_name),	/* #16-#17 */
    /* no copy here, pass by ref */ sparp->sparp_env->spare_sql_refresh_free_text, /* #18 */
    (ptrlong)use_plain_return,	/* #19 */
    t_box_num (rgc_flags) );	/* #20 */
}

void
ssg_grabber_codegen (struct spar_sqlgen_s *ssg, struct spar_tree_s *spart, ...)
{
  int argctr = 0;
/* The order of declarations is important: side effect on init */
  SPART **retvals		= (SPART **)(spart->_.codegen.args [argctr++]);	/* #2 */
  caddr_t procedure_name	= (caddr_t)(spart->_.codegen.args [argctr++]);	/* #3 */
  caddr_t seed_sql_text		= (caddr_t)(spart->_.codegen.args [argctr++]);	/* #4 */
  caddr_t iter_sql_text		= (caddr_t)(spart->_.codegen.args [argctr++]);	/* #5 */
  caddr_t final_sql_text	= (caddr_t)(spart->_.codegen.args [argctr++]);	/* #6 */
  caddr_t ret_limit		= (caddr_t)(spart->_.codegen.args [argctr++]);	/* #7 */
  SPART *const_vector_expn	= (SPART *)(spart->_.codegen.args [argctr++]);	/* #8 */
  SPART *sa_graphs_vector_expn	= (SPART *)(spart->_.codegen.args [argctr++]);	/* #9 */
  SPART *sa_preds_vector_expn	= (SPART *)(spart->_.codegen.args [argctr++]);	/* #10 */
  caddr_t depth			= (caddr_t)(spart->_.codegen.args [argctr++]);	/* #11 */
  caddr_t grab_limit		= (caddr_t)(spart->_.codegen.args [argctr++]);	/* #12 */
  caddr_t base			= (caddr_t)(spart->_.codegen.args [argctr++]);	/* #13 */
  caddr_t destination		= (caddr_t)(spart->_.codegen.args [argctr++]);	/* #14 */
  caddr_t group_destination	= (caddr_t)(spart->_.codegen.args [argctr++]);	/* #15 */
  caddr_t resolver_name		= (caddr_t)(spart->_.codegen.args [argctr++]);	/* #16 */
  caddr_t loader_name		= (caddr_t)(spart->_.codegen.args [argctr++]);	/* #17 */
  int refresh_free_text		= (ptrlong)(spart->_.codegen.args [argctr++]);	/* #18 */
  int use_plain_return		= (ptrlong)(spart->_.codegen.args [argctr++]);	/* #19 */
  caddr_t rgc_flags		= (caddr_t)(spart->_.codegen.args [argctr++]);	/* #20 */
  int varctr, varcount = BOX_ELEMENTS (retvals);
  int need_comma;
  caddr_t call_alias = t_box_sprintf (0x100, "grabber-t%d", ssg->ssg_sparp->sparp_key_gen);
  ssg->ssg_sparp->sparp_key_gen += 1;
  if (NULL == const_vector_expn)
    const_vector_expn = (SPART *)t_NEW_DB_NULL;
  if (NULL == sa_graphs_vector_expn)
    sa_graphs_vector_expn = (SPART *)t_NEW_DB_NULL;
  if (NULL == sa_preds_vector_expn)
    sa_preds_vector_expn = (SPART *)t_NEW_DB_NULL;
  if (NULL == depth)
    depth = (caddr_t)1L;
  if (NULL == grab_limit)
    grab_limit = t_box_num (MAX_BOX_ELEMENTS);
  if (NULL == base)
    base = t_NEW_DB_NULL;
  if (NULL == destination)
    destination = t_NEW_DB_NULL;
  if (NULL == group_destination)
    group_destination = t_NEW_DB_NULL;
  if (NULL == resolver_name)
    resolver_name = t_box_dv_short_string ("DB.DBA.RDF_GRAB_RESOLVER_DEFAULT");
  if (NULL == loader_name)
    loader_name = t_box_dv_short_string ("DB.DBA.RDF_SPONGE_UP");
  if (use_plain_return)
    {
      ssg_puts ("SELECT TOP 1 ");
      ssg_prin_function_name (ssg, procedure_name);
      ssg_puts (" (");
      ssg->ssg_indent += 1;
    }
  else
    {
  ssg_puts ("SELECT ");
      if (0 == varcount) ssg_puts ("TOP 1 1 as __ask_retval "); /* IvAn/Bug12961/071109 Added special case for ASK */
  for (varctr = 0; varctr < varcount; varctr++)
    {
      char buf[30];
      char *asname;
      if (varctr)
        ssg_puts (", ");
      ssg_prin_id (ssg, call_alias);
      sprintf (buf, ".rset[%d] as ", varctr);
      ssg_puts (buf);
      asname = spar_alias_name_of_ret_column (retvals [varctr]);
      if (NULL == asname)
        {
          sprintf (buf, "callret-%d", varctr);
          asname = buf;
        }
      ssg_prin_id (ssg, asname);
    }
  ssg_newline (0);
  ssg_puts ("FROM ");
  ssg_prin_function_name (ssg, procedure_name);
      ssg_puts (" (_grabber_params, _grabber_seed, _grabber_iter, _grabber_final, _grabber_ret_limit, _grabber_consts, _grabber_sa_graphs, _grabber_sa_preds, _grabber_depth, _grabber_doc_limit, _grabber_base, _grabber_destination, _grabber_group_destination, _grabber_resolver, _grabber_loader, _refresh_free_text, _plain_ret, _grabber_flags) (rset any) ");
  ssg_prin_id (ssg, call_alias);
      ssg_newline (0);
      ssg_puts ("WHERE _grabber_params = ");
    }
  need_comma = 0;
  ssg_puts ("vector (");
  DO_SET (caddr_t, vname, &(ssg->ssg_sparp->sparp_env->spare_global_var_names))
    {
      if (need_comma)
        ssg_puts (", ");
      else
        need_comma = 1;
      ssg_print_global_param (ssg, vname, SSG_VALMODE_SQLVAL);
    }
  END_DO_SET ();
  ssg_puts (")");
#define PROC_PARAM_EQ_SPART(txt,var) do { \
    if (use_plain_return) \
      { \
        ssg_puts (", "); \
        ssg_newline (0); \
      } \
    else \
      { \
    ssg_newline (0); \
        ssg_puts (" AND "); \
    ssg_puts (txt); \
        ssg_puts (" = "); \
      } \
    ssg_print_scalar_expn (ssg, (SPART *)(var), SSG_VALMODE_SQLVAL, NULL_ASNAME); \
    } while (0)
  PROC_PARAM_EQ_SPART ("_grabber_seed", seed_sql_text);
  PROC_PARAM_EQ_SPART ("_grabber_iter", iter_sql_text);
  PROC_PARAM_EQ_SPART ("_grabber_final", final_sql_text);
  PROC_PARAM_EQ_SPART ("_grabber_ret_limit", ret_limit);
  PROC_PARAM_EQ_SPART ("_grabber_consts", const_vector_expn);
  PROC_PARAM_EQ_SPART ("_grabber_sa_graphs", sa_graphs_vector_expn);
  PROC_PARAM_EQ_SPART ("_grabber_sa_preds", sa_preds_vector_expn);
  PROC_PARAM_EQ_SPART ("_grabber_depth", depth);
  PROC_PARAM_EQ_SPART ("_grabber_doc_limit", grab_limit);
  PROC_PARAM_EQ_SPART ("_grabber_base", base);
  PROC_PARAM_EQ_SPART ("_grabber_destination", destination);
  PROC_PARAM_EQ_SPART ("_grabber_group_destination", group_destination);
  PROC_PARAM_EQ_SPART ("_grabber_resolver", resolver_name);
  PROC_PARAM_EQ_SPART ("_grabber_loader", loader_name);
  PROC_PARAM_EQ_SPART ("_refresh_free_text", (ptrlong) refresh_free_text);
  PROC_PARAM_EQ_SPART ("_plain_ret", ((ptrlong) use_plain_return));
  PROC_PARAM_EQ_SPART ("_grabber_flags", rgc_flags);
#undef PROC_PARAM_EQ_SPART
  if (use_plain_return)
    {
      ssg_putchar (')');
      ssg->ssg_indent -= 1;
      ssg_newline (0);
      ssg_puts ("FROM DB.DBA.RDF_QUAD ");
      ssg_prin_id (ssg, call_alias);
    }
}
