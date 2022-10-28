/** -------------------------------------------
 * @file   exec.c
 * @brief  Emfrp REPL Interpreter Implementation
 * @author Go Suzuki <puyogo.suzuki@gmail.com>
 * @date   2022/10/28
 ------------------------------------------- */

#include "vm/exec.h"

em_result
exec_ast_bin(machine_t * m, parser_expression_kind_t kind, parser_expression_t * l, parser_expression_t * r, object_t ** out) {
  object_t * lro = nullptr, * rro = nullptr;
  bool lro_is_integer, rro_is_integer;
  em_result errres;
  CHKERR(exec_ast(m, l, &lro));

  if(kind == EXPR_KIND_DAND && lro == &object_false) {
    *out = &object_false;
    return EM_RESULT_OK;
  } else if(kind == EXPR_KIND_DOR && lro != &object_false) {
    *out = lro;
    return EM_RESULT_OK;
  }
  
  CHKERR(exec_ast(m, r, &rro));
  
  lro_is_integer = object_is_integer(lro);
  rro_is_integer = object_is_integer(rro);

  if(lro_is_integer && rro_is_integer) {
    int32_t retVal = 0;
    int32_t ll = object_get_integer(lro);
    int32_t rr = object_get_integer(rro);
    switch(kind) {
    case EXPR_KIND_ADDITION: retVal = ll + rr; break;
    case EXPR_KIND_SUBTRACTION: retVal = ll - rr; break;
    case EXPR_KIND_DIVISION: retVal = ll / rr; break;
    case EXPR_KIND_MULTIPLICATION: retVal = ll * rr; break;
    case EXPR_KIND_MODULO: retVal = ll % rr; break;
    case EXPR_KIND_LEFT_SHIFT: retVal = ll << rr; break;
    case EXPR_KIND_RIGHT_SHIFT: retVal = ll >> rr; break;
    case EXPR_KIND_LESS_OR_EQUAL: retVal = ll <= rr; break;
    case EXPR_KIND_LESS_THAN: retVal = ll < rr; break;
    case EXPR_KIND_GREATER_OR_EQUAL: retVal = ll >= rr; break;
    case EXPR_KIND_GREATER_THAN: retVal = ll > rr; break;
    case EXPR_KIND_EQUAL: retVal = ll == rr; break;
    case EXPR_KIND_NOT_EQUAL: retVal = ll != rr; break;
    case EXPR_KIND_AND: retVal = ll & rr; break;
    case EXPR_KIND_OR: retVal = ll | rr; break;
    case EXPR_KIND_XOR: retVal = ll ^ rr; break;
    default: DEBUGBREAK; break;
    }
    if(kind >= EXPR_KIND_LESS_OR_EQUAL && kind <= EXPR_KIND_GREATER_THAN)
      *out = retVal ? &object_true : &object_false;
    else
      object_new_int(out, retVal);
    return EM_RESULT_OK;
  } else {
    switch(kind) {
    case EXPR_KIND_EQUAL:
      if(lro->kind != rro->kind) {
	*out = &object_false;
	goto ok;
      }
      // Add here if you added the new type.
      *out = &object_true;
      goto ok;
    case EXPR_KIND_NOT_EQUAL:
      if(lro->kind != rro->kind) {
	*out = &object_true;
	goto ok;
      }
      // Add here if you added the new type.
      *out = &object_true;
      goto ok;
    case EXPR_KIND_AND: case EXPR_KIND_DAND: // Code size than performance.
      *out = (lro != &object_false) && (rro != &object_false) ? &object_true : &object_false;
      goto ok;
    case EXPR_KIND_OR: case EXPR_KIND_DOR: // Code size than performance.
      *out = (lro != &object_false) || (rro != &object_false) ? &object_true : &object_false;
      goto ok;
    case EXPR_KIND_XOR:
      *out = (lro != &object_false) ^ (rro != &object_false) ? &object_true : &object_false;
      goto ok;
    default:
      object_free(lro);
      object_free(rro);
      return EM_RESULT_TYPE_MISMATCH;
    }
  ok:
    object_free(lro);
    object_free(rro);
    return EM_RESULT_OK;
  }

 err:
  return errres;
}

//em_result
//exec_ast_unary(

em_result
exec_ast(machine_t * m, parser_expression_t * v, object_t ** out) {
  em_result errres;
  if (EXPR_KIND_IS_INTEGER(v))
    object_new_int(out, (int32_t)((size_t)v >> 1));
  else if (EXPR_KIND_IS_BOOLEAN(v)) {
    *out = EXPR_IS_TRUE(v) ? &object_true : &object_false;
  } else if (EXPR_KIND_IS_BIN_OP(v)) {
    CHKERR(exec_ast_bin(m, v->kind, v->value.binary.lhs, v->value.binary.rhs, out));
  } else {
    switch(v->kind) {
    case EXPR_KIND_IF: {
      object_t * cond_result = nullptr;
      bool cond_v;
      CHKERR(exec_ast(m, v->value.ifthenelse.cond, &cond_result));
      cond_v = cond_result == &object_true;
      object_free(cond_result);
      CHKERR(exec_ast(m, cond_v ? v->value.ifthenelse.then : v->value.ifthenelse.otherwise, out));
      break;
    }
    case EXPR_KIND_IDENTIFIER:
      if(m->executing_node_name != nullptr && string_compare(&(v->value.identifier), m->executing_node_name))
        return EM_RESULT_CYCLIC_REFERENCE;
      if(!machine_search_node(m, out, &(v->value.identifier)))
        return EM_RESULT_MISSING_IDENTIFIER;
      break;
    case EXPR_KIND_LAST_IDENTIFIER:
      if(m->executing_node_name != nullptr && !string_compare(&(v->value.identifier), m->executing_node_name))
        return EM_RESULT_MISSING_IDENTIFIER;
      if(!machine_search_node(m, out, &(v->value.identifier)))
        return EM_RESULT_MISSING_IDENTIFIER;
      break;
    default:
      return EM_RESULT_INVALID_ARGUMENT;
    }
  }
  return EM_RESULT_OK;
 err:
  return errres;
}

em_result
get_dependencies_ast(parser_expression_t * v, list_t /*<string_t *>*/ ** out) {
  em_result errres;
  if(EXPR_KIND_IS_INTEGER(v) || EXPR_KIND_IS_BOOLEAN(v)) return EM_RESULT_OK;
  if(v->kind == EXPR_KIND_IDENTIFIER) {
    string_t * s = &(v->value.identifier);
    CHKERR(list_add2(out, string_t *, &s));
  } else if(EXPR_KIND_IS_BIN_OP(v)){
    CHKERR(get_dependencies_ast(v->value.binary.lhs, out));
    CHKERR(get_dependencies_ast(v->value.binary.rhs, out));
  }
  return EM_RESULT_OK;
 err:
  return errres;
}

bool
check_depends_on_ast(parser_expression_t * v, string_t * str) {
  if (EXPR_KIND_IS_INTEGER(v) || EXPR_KIND_IS_BOOLEAN(v)) return false;
  if(v->kind == EXPR_KIND_IDENTIFIER)
    return string_compare(&(v->value.identifier), str);
  else if(EXPR_KIND_IS_BIN_OP(v))
    return check_depends_on_ast(v->value.binary.lhs, str) || check_depends_on_ast(v->value.binary.rhs, str);
  return false;
}
