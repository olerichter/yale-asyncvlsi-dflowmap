/*
 * This file is part of the ACT library
 *
 * Copyright (c) 2021 Rui Li
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "ProcGenerator.h"

const char *ProcGenerator::getActIdOrCopyName(ActId *actId) {
  char *str = new char[10240];
  if (actId) {
    char *actName = new char[10240];
    getActIdName(sc, actId, actName, 10240);
    unsigned outUses = getOpUses(actId);
    if (debug_verbose) {
      printf("actIdCopyUse (%s, %u)\n", actName, outUses);
    }
    if (outUses) {
      unsigned copyUse = getCopyUses(actId);
      if (debug_verbose) {
        printf("for %s, outUses: %d, copyUse: %d\n", actName, outUses, copyUse);
      }
      if (copyUse <= outUses) {
        const char *normalizedName = getNormActIdName(actName);
        sprintf(str, "%scopy.out[%u]", normalizedName, copyUse);
      } else {
        printf("We use %s more than total uses!\n", actName);
        exit(-1);
      }
    } else {
      sprintf(str, "%s", actName);
    }
  } else {
    sprintf(str, "*");
  }
  return str;
}

void ProcGenerator::collectBitwidthInfo() {
  ActInstiter inst(p->CurScope());
  for (inst = inst.begin(); inst != inst.end(); inst++) {
    ValueIdx *vx = *inst;
    act_connection *c;
    const char *varName = vx->getName();
    auto tmp = ActId::parseId(vx->getName());
    c = tmp->Canonical(p->CurScope());
    delete tmp;
    int bitwidth = TypeFactory::bitWidth(vx->t);
    if (bitwidth <= 0) {
      if (debug_verbose) {
        printf("%s has negative bw %d!\n", varName, bitwidth);
      }
    } else {
      if (debug_verbose) {
        printf("Update bitwidthMap for (%s, %d).\n", varName, bitwidth);
      }
      bitwidthMap.insert({c, bitwidth});
    }
  }
}

void ProcGenerator::printBitwidthInfo() {
  printf("bitwidth info:\n");
  for (auto &bitwidthMapIt: bitwidthMap) {
    char *connectName = new char[10240];
    getActConnectionName(bitwidthMapIt.first, connectName, 10240);
    printf("(%s, %u) ", connectName, bitwidthMapIt.second);
  }
  printf("\n");
}

unsigned ProcGenerator::getActIdBW(ActId *actId) {
  act_connection *c = actId->Canonical(p->CurScope());
  unsigned bw = getBitwidth(c);
  if (debug_verbose) {
    printf("Fetch BW for actID ");
    actId->Print(stdout);
    printf(": %u\n", bw);
  }
  return bw;
}

unsigned ProcGenerator::getBitwidth(act_connection *actConnection) {
  auto bitwidthMapIt = bitwidthMap.find(actConnection);
  if (bitwidthMapIt != bitwidthMap.end()) {
    return bitwidthMapIt->second;
  }
  char *varName = new char[10240];
  getActConnectionName(actConnection, varName, 10240);
  printf("We could not find bitwidth info for %s\n", varName);
  printBitwidthInfo();
  exit(-1);
}

const char *ProcGenerator::EMIT_QUERY(DflowGenerator *dflowGenerator,
                                      Expr *expr,
                                      const char *sym,
                                      char *procName,
                                      int &resSuffix,
                                      unsigned &resBW) {
  if (debug_verbose) {
    printf("handle query expression for ");
    print_expr(stdout, expr);
    printf("\n");
  }
  Expr *cExpr = expr->u.e.l;
  Expr *lExpr = expr->u.e.r->u.e.l;
  Expr *rExpr = expr->u.e.r->u.e.r;
  if (procName[0] == '\0') {
    sprintf(procName, "func");
  }
  unsigned cBW = 1;
  const char *cexpr_name = printExpr(dflowGenerator,
                                     cExpr,
                                     procName,
                                     resSuffix,
                                     cBW);
  const char *lexpr_name = printExpr(dflowGenerator,
                                     lExpr,
                                     procName,
                                     resSuffix,
                                     resBW);
  const char *rexpr_name = printExpr(dflowGenerator,
                                     rExpr,
                                     procName,
                                     resSuffix,
                                     resBW);
  {
    /* generate subProc name */
    char *cVal = new char[100];
    getCurProc(cexpr_name, cVal);
    char *lVal = new char[100];
    getCurProc(lexpr_name, lVal);
    char *rVal = new char[100];
    getCurProc(rexpr_name, rVal);
    if (!strcmp(lexpr_name, rexpr_name)) {
      printf("This query expr has the same true/false branch!\n");
      print_expr(stdout, expr);
      printf("!\n");
      exit(-1);
    }
    char *subProcName = new char[1500];
    sprintf(subProcName, "_%s%s%s%s", lVal, sym, cVal, rVal);
    strcat(procName, subProcName);
  }
  char *finalExprName = new char[100];
  resSuffix++;
  sprintf(finalExprName, "res%d", resSuffix);
  dflowGenerator->printChpQueryExpr(cexpr_name,
                                    lexpr_name,
                                    rexpr_name,
                                    resSuffix,
                                    resBW);
  /* create Expr */
  int cType = (cExpr->type == E_INT) ? E_INT : E_VAR;
  int lType = (lExpr->type == E_INT) ? E_INT : E_VAR;
  int rType = (rExpr->type == E_INT) ? E_INT : E_VAR;
  int exprType = expr->type;
  int bodyExprType = expr->u.e.r->type;
  dflowGenerator->prepareQueryExprForOpt(cexpr_name,
                                         cType,
                                         lexpr_name,
                                         lType,
                                         rexpr_name,
                                         rType,
                                         finalExprName,
                                         exprType,
                                         bodyExprType,
                                         resBW);
  return finalExprName;
}

const char *ProcGenerator::EMIT_BIN(DflowGenerator *dflowGenerator,
                                    Expr *expr,
                                    const char *sym,
                                    const char *op,
                                    int type,
                                    char *procName,
                                    int &resSuffix,
                                    unsigned &resBW) {
  if (debug_verbose) {
    printf("Handle bin expr ");
    print_expr(stdout, expr);
    printf("\n");
  }
  Expr *lExpr = expr->u.e.l;
  Expr *rExpr = expr->u.e.r;
  if (procName[0] == '\0') {
    sprintf(procName, "func");
  }
  const char *lexpr_name = printExpr(dflowGenerator,
                                     lExpr,
                                     procName,
                                     resSuffix,
                                     resBW);
  const char *rexpr_name = printExpr(dflowGenerator,
                                     rExpr,
                                     procName,
                                     resSuffix,
                                     resBW);
  {
    /* generate subProc name */
    char *lVal = new char[100];
    getCurProc(lexpr_name, lVal);
    char *rVal = new char[100];
    getCurProc(rexpr_name, rVal);
    char *subProcName = new char[1500];
    if (!strcmp(lexpr_name, rexpr_name)) {
      sprintf(subProcName, "_%s%s2%s", lVal, sym, rVal);
    } else {
      sprintf(subProcName, "_%s%s%s", lVal, sym, rVal);
    }
    strcat(procName, subProcName);
  }
  char *finalExprName = new char[100];
  resSuffix++;
  sprintf(finalExprName, "res%d", resSuffix);
  dflowGenerator->printChpBinExpr(op,
                                  lexpr_name,
                                  rexpr_name,
                                  type,
                                  resSuffix,
                                  resBW);
  int lType = (lExpr->type == E_INT) ? E_INT : E_VAR;
  int rType = (rExpr->type == E_INT) ? E_INT : E_VAR;
  int exprType = expr->type;
  dflowGenerator->prepareBinExprForOpt(lexpr_name,
                                       lType,
                                       rexpr_name,
                                       rType,
                                       finalExprName,
                                       exprType,
                                       resBW);
  return finalExprName;
}

const char *ProcGenerator::EMIT_UNI(DflowGenerator *dflowGenerator,
                                    Expr *expr,
                                    const char *sym,
                                    const char *op,
                                    char *procName,
                                    int &resSuffix,
                                    unsigned &resBW) {
  if (debug_verbose) {
    printf("Handle uni expr ");
    print_expr(stdout, expr);
    printf("\n");
  }
  Expr *lExpr = expr->u.e.l;
  if (procName[0] == '\0') {
    sprintf(procName, "func");
  }
  const char *lexpr_name = printExpr(dflowGenerator,
                                     lExpr,
                                     procName,
                                     resSuffix,
                                     resBW);
  {
    /* generate subProc name */
    char *val = new char[100];
    getCurProc(lexpr_name, val);
    sprintf(procName, "%s_%s%s", procName, sym, val);
  }
  char *finalExprName = new char[100];
  resSuffix++;
  sprintf(finalExprName, "res%d", resSuffix);
  dflowGenerator->printChpUniExpr(op, lexpr_name, resSuffix, resBW);
  int lType = (lExpr->type == E_INT) ? E_INT : E_VAR;
  int exprType = expr->type;
  dflowGenerator->prepareUniExprForOpt(lexpr_name,
                                       lType,
                                       finalExprName,
                                       exprType,
                                       resBW);
  return finalExprName;
}

const char *ProcGenerator::printExpr(DflowGenerator *dflowGenerator,
                                     Expr *expr,
                                     char *procName,
                                     int &resSuffix,
                                     unsigned &resBW) {
  int type = expr->type;
  switch (type) {
    case E_INT: {
      unsigned long val = expr->u.v;
      const char *valStr = strdup(std::to_string(val).c_str());
      return valStr;
    }
    case E_VAR: {
      auto actId = (ActId *) expr->u.e.l;
      act_connection *actConnection = actId->Canonical(sc);
      unsigned argBW = getBitwidth(actConnection);
      char *oriVarName = new char[10240];
      getActIdName(sc, actId, oriVarName, 10240);
      const char *mappedVarName = nullptr;
      if (dflowGenerator->isNewArg(oriVarName)) {
        mappedVarName = getActIdOrCopyName(actId);
      }
      if (resBW == 0) {
        resBW = argBW;
      }
      if (debug_verbose) {
        printf("oriVarName: %s, mappedVarName: %s, res_bw: %u\n",
               oriVarName, mappedVarName, resBW);
      }
      return dflowGenerator->handleEVar(oriVarName, mappedVarName, argBW);
    }
    case E_AND: {
      return EMIT_BIN(dflowGenerator,
                      expr,
                      "and",
                      "&",
                      type,
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_OR: {
      return EMIT_BIN(dflowGenerator,
                      expr,
                      "or",
                      "|",
                      type,
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_NOT: {
      return EMIT_UNI(dflowGenerator,
                      expr,
                      "not",
                      "~",
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_PLUS: {
      return EMIT_BIN(dflowGenerator,
                      expr,
                      "add",
                      "+",
                      type,
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_MINUS: {
      return EMIT_BIN(dflowGenerator,
                      expr,
                      "minus",
                      "-",
                      type,
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_MULT: {
      return EMIT_BIN(dflowGenerator,
                      expr,
                      "mul",
                      "*",
                      type,
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_DIV: {
      return EMIT_BIN(dflowGenerator,
                      expr,
                      "div",
                      "/",
                      type,
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_MOD: {
      return EMIT_BIN(dflowGenerator,
                      expr,
                      "mod",
                      "%",
                      type,
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_LSL: {
      return EMIT_BIN(dflowGenerator,
                      expr,
                      "lsl",
                      "<<",
                      type,
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_LSR: {
      return EMIT_BIN(dflowGenerator,
                      expr,
                      "lsr",
                      ">>",
                      type,
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_ASR: {
      return EMIT_BIN(dflowGenerator,
                      expr,
                      "asr",
                      ">>>",
                      type,
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_UMINUS: {
      return EMIT_UNI(dflowGenerator,
                      expr,
                      "neg",
                      "-",
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_XOR: {
      return EMIT_BIN(dflowGenerator,
                      expr,
                      "xor",
                      "^",
                      type,
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_LT: {
      return EMIT_BIN(dflowGenerator,
                      expr,
                      "lt",
                      "<",
                      type,
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_GT: {
      return EMIT_BIN(dflowGenerator,
                      expr,
                      "gt",
                      ">",
                      type,
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_LE: {
      return EMIT_BIN(dflowGenerator,
                      expr,
                      "le",
                      "<=",
                      type,
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_GE: {
      return EMIT_BIN(dflowGenerator,
                      expr,
                      "ge",
                      ">=",
                      type,
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_EQ: {
      return EMIT_BIN(dflowGenerator,
                      expr,
                      "eq",
                      "=",
                      type,
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_NE: {
      return EMIT_BIN(dflowGenerator,
                      expr,
                      "ne",
                      "!=",
                      type,
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_COMPLEMENT: {
      return EMIT_UNI(dflowGenerator,
                      expr,
                      "compl",
                      "~",
                      procName,
                      resSuffix,
                      resBW);
    }
    case E_BUILTIN_INT: {
      Expr *lExpr = expr->u.e.l;
      if (resBW == 0) {
        Expr *rExpr = expr->u.e.r;
        if (rExpr) {
          resBW = rExpr->u.v;
        } else {
          resBW = 1;
        }
      }
      if (debug_verbose) {
        printf("It is E_BUILTIN_INT! The real expression is ");
        print_expr(stdout, lExpr);
        printf(", resBW: %u\n", resBW);
      }
      return printExpr(dflowGenerator,
                       lExpr,
                       procName,
                       resSuffix,
                       resBW);
    }
    case E_BUILTIN_BOOL: {
      Expr *lExpr = expr->u.e.l;
      resBW = 1;
      return printExpr(dflowGenerator,
                       lExpr,
                       procName,
                       resSuffix,
                       resBW);
    }
    case E_QUERY: {
      return EMIT_QUERY(dflowGenerator,
                        expr,
                        "q",
                        procName,
                        resSuffix,
                        resBW);
    }
    default: {
      print_expr(stdout, expr);
      printf("\n");
      printf("when printing expression, encounter unknown expression type %d\n",
             type);
      exit(-1);
    }
  }
  printf("Shouldn't be here");
  exit(-1);
}

unsigned ProcGenerator::getCopyUses(ActId *actId) {
  act_connection *actConnection = actId->Canonical(sc);
  auto copyUsesIt = copyUses.find(actConnection);
  if (copyUsesIt == copyUses.end()) {
    char buf[10240];
    getActConnectionName(actConnection, buf, 10240);
    printf("We don't know how many times %s is used as COPY!\n", buf);
    exit(-1);
  }
  unsigned uses = copyUsesIt->second;
  copyUsesIt->second++;
  return uses;
}

void ProcGenerator::updateOpUses(act_connection *actConnection) {
  auto opUsesIt = opUses.find(actConnection);
  if (opUsesIt == opUses.end()) {
    opUses.insert({actConnection, 0});
    copyUses.insert({actConnection, 0});
  } else {
    opUsesIt->second++;
  }
}

void ProcGenerator::updateOpUses(ActId *actId) {
  act_connection *actConnection = actId->Canonical(sc);
  updateOpUses(actConnection);
}

void ProcGenerator::recordOpUses(ActId *actId,
                                 ActConnectVec &actConnectVec) {
  act_connection *actConnection = actId->Canonical(sc);
  if (std::find(actConnectVec.begin(), actConnectVec.end(), actConnection) ==
      actConnectVec.end()) {
    actConnectVec.push_back(actConnection);
  }
}

void ProcGenerator::printOpUses() {
  printf("OP USES:\n");
  for (auto &opUsesIt: opUses) {
    char *opName = new char[10240];
    getActConnectionName(opUsesIt.first, opName, 10240);
    printf("(%s, %u) ", opName, opUsesIt.second);
  }
  printf("\n");
}

bool ProcGenerator::isOpUsed(ActId *actId) {
  act_connection *actConnection = actId->Canonical(sc);
  return opUses.find(actConnection) != opUses.end();
}

unsigned ProcGenerator::getOpUses(ActId *actId) {
  act_connection *actConnection = actId->Canonical(sc);
  auto opUsesIt = opUses.find(actConnection);
  if (opUsesIt != opUses.end()) {
    return opUsesIt->second;
  }
  char *buf = new char[10240];
  getActConnectionName(actConnection, buf, 10240);
  printf("We don't know how many times %s is used!\n", buf);
  printOpUses();
  exit(-1);
}

void ProcGenerator::collectUniOpUses(Expr *expr, StringVec &recordedOps) {
  Expr *lExpr = expr->u.e.l;
  collectExprUses(lExpr, recordedOps);
}

void ProcGenerator::collectBinOpUses(Expr *expr, StringVec &recordedOps) {
  Expr *lExpr = expr->u.e.l;
  collectExprUses(lExpr, recordedOps);
  Expr *rExpr = expr->u.e.r;
  collectExprUses(rExpr, recordedOps);
}

void ProcGenerator::recordUniOpUses(Expr *expr,
                                    ActConnectVec &actConnectVec) {
  Expr *lExpr = expr->u.e.l;
  recordExprUses(lExpr, actConnectVec);
}

void ProcGenerator::recordBinOpUses(Expr *expr,
                                    ActConnectVec &actConnectVec) {
  Expr *lExpr = expr->u.e.l;
  recordExprUses(lExpr, actConnectVec);
  Expr *rExpr = expr->u.e.r;
  recordExprUses(rExpr, actConnectVec);
}

void ProcGenerator::collectExprUses(Expr *expr, StringVec &recordedOps) {
  int type = expr->type;
  switch (type) {
    case E_AND:
    case E_OR:
    case E_PLUS:
    case E_MINUS:
    case E_MULT:
    case E_DIV:
    case E_MOD:
    case E_LSL:
    case E_LSR:
    case E_ASR:
    case E_XOR:
    case E_LT:
    case E_GT:
    case E_LE:
    case E_GE:
    case E_EQ:
    case E_NE: {
      collectBinOpUses(expr, recordedOps);
      break;
    }
    case E_NOT:
    case E_UMINUS:
    case E_COMPLEMENT: {
      collectUniOpUses(expr, recordedOps);
      break;
    }
    case E_INT: {
      break;
    }
    case E_VAR: {
      auto actId = (ActId *) expr->u.e.l;
      char *varName = new char[10240];
      getActIdName(sc, actId, varName, 10240);
      if (searchStringVec(recordedOps, varName) == -1) {
        updateOpUses(actId);
        recordedOps.push_back(varName);
      }
      break;
    }
    case E_BUILTIN_INT: {
      Expr *lExpr = expr->u.e.l;
      collectExprUses(lExpr, recordedOps);
      break;
    }
    case E_BUILTIN_BOOL: {
      Expr *lExpr = expr->u.e.l;
      collectExprUses(lExpr, recordedOps);
      break;
    }
    case E_QUERY: {
      Expr *cExpr = expr->u.e.l;
      Expr *lExpr = expr->u.e.r->u.e.l;
      Expr *rExpr = expr->u.e.r->u.e.r;
      collectExprUses(cExpr, recordedOps);
      collectExprUses(lExpr, recordedOps);
      collectExprUses(rExpr, recordedOps);
      break;
    }
    default: {
      print_expr(stdout, expr);
      printf("\nUnknown expression type %d when collecting expr use\n", type);
      exit(-1);
    }
  }
}

void ProcGenerator::recordExprUses(Expr *expr,
                                   ActConnectVec &actConnectVec) {
  int type = expr->type;
  switch (type) {
    case E_AND:
    case E_OR:
    case E_PLUS:
    case E_MINUS:
    case E_MULT:
    case E_DIV:
    case E_MOD:
    case E_LSL:
    case E_LSR:
    case E_ASR:
    case E_XOR:
    case E_LT:
    case E_GT:
    case E_LE:
    case E_GE:
    case E_EQ:
    case E_NE: {
      recordBinOpUses(expr, actConnectVec);
      break;
    }
    case E_NOT:
    case E_UMINUS:
    case E_COMPLEMENT: {
      recordUniOpUses(expr, actConnectVec);
      break;
    }
    case E_INT: {
      break;
    }
    case E_VAR: {
      auto actId = (ActId *) expr->u.e.l;
      recordOpUses(actId, actConnectVec);
      break;
    }
    case E_BUILTIN_INT: {
      Expr *lExpr = expr->u.e.l;
      recordExprUses(lExpr, actConnectVec);
      break;
    }
    case E_BUILTIN_BOOL: {
      Expr *lExpr = expr->u.e.l;
      recordExprUses(lExpr, actConnectVec);
      break;
    }
    case E_QUERY: {
      Expr *cExpr = expr->u.e.l;
      Expr *lExpr = expr->u.e.r->u.e.l;
      Expr *rExpr = expr->u.e.r->u.e.r;
      recordExprUses(cExpr, actConnectVec);
      recordExprUses(lExpr, actConnectVec);
      recordExprUses(rExpr, actConnectVec);
      break;
    }
    default: {
      print_expr(stdout, expr);
      printf("\nUnknown expression type %d when recording expr use\n", type);
      exit(-1);
    }
  }
}

void ProcGenerator::collectDflowClusterUses(list_t *dflow,
                                            ActConnectVec &actConnectVec) {
  listitem_t *li;
  for (li = list_first (dflow); li; li = list_next (li)) {
    auto *d = (act_dataflow_element *) list_value (li);
    switch (d->t) {
      case ACT_DFLOW_FUNC: {
        Expr *expr = d->u.func.lhs;
        recordExprUses(expr, actConnectVec);
        break;
      }
      case ACT_DFLOW_SPLIT: {
        ActId *input = d->u.splitmerge.single;
        recordOpUses(input, actConnectVec);
        ActId *guard = d->u.splitmerge.guard;
        recordOpUses(guard, actConnectVec);
        break;
      }
      case ACT_DFLOW_MERGE: {
        ActId *guard = d->u.splitmerge.guard;
        recordOpUses(guard, actConnectVec);
        int numInputs = d->u.splitmerge.nmulti;
        if (numInputs < 2) {
          dflow_print(stdout, d);
          printf("\nMerge has less than TWO inputs!\n");
          exit(-1);
        }
        ActId **inputs = d->u.splitmerge.multi;
        for (int i = 0; i < numInputs; i++) {
          ActId *in = inputs[i];
          recordOpUses(in, actConnectVec);
        }
        break;
      }
      case ACT_DFLOW_MIXER:
      case ACT_DFLOW_ARBITER: {
        int numInputs = d->u.splitmerge.nmulti;
        if (numInputs < 2) {
          dflow_print(stdout, d);
          printf(" has less than TWO inputs!\n");
          exit(-1);
        }
        ActId **inputs = d->u.splitmerge.multi;
        for (int i = 0; i < numInputs; i++) {
          ActId *in = inputs[i];
          recordOpUses(in, actConnectVec);
        }
        break;
      }
      case ACT_DFLOW_CLUSTER: {
        printf("Do not support nested dflow_cluster!\n");
        exit(-1);
      }
      case ACT_DFLOW_SINK: {
        printf("dflow cluster should not connect to SINK!\n");
        exit(-1);
      }
      default: {
        printf("Unknown dataflow type %d\n", d->t);
        exit(-1);
      }
    }
  }
}

void ProcGenerator::collectOpUses() {
  listitem_t *li;
  for (li = list_first (p->getlang()->getdflow()->dflow);
       li;
       li = list_next (li)) {
    auto *d = (act_dataflow_element *) list_value (li);
    switch (d->t) {
      case ACT_DFLOW_SINK: {
        ActId *input = d->u.sink.chan;
        updateOpUses(input);
        break;
      }
      case ACT_DFLOW_FUNC: {
        Expr *expr = d->u.func.lhs;
        StringVec recordedOps;
        collectExprUses(expr, recordedOps);
        break;
      }
      case ACT_DFLOW_SPLIT: {
        ActId *input = d->u.splitmerge.single;
        updateOpUses(input);
        ActId *guard = d->u.splitmerge.guard;
        updateOpUses(guard);
        break;
      }
      case ACT_DFLOW_MERGE: {
        ActId *guard = d->u.splitmerge.guard;
        updateOpUses(guard);
        int numInputs = d->u.splitmerge.nmulti;
        if (numInputs < 2) {
          dflow_print(stdout, d);
          printf("\nMerge has less than TWO inputs!\n");
          exit(-1);
        }
        ActId **inputs = d->u.splitmerge.multi;
        for (int i = 0; i < numInputs; i++) {
          ActId *in = inputs[i];
          updateOpUses(in);
        }
        break;
      }
      case ACT_DFLOW_MIXER:
      case ACT_DFLOW_ARBITER: {
        int numInputs = d->u.splitmerge.nmulti;
        if (numInputs < 2) {
          dflow_print(stdout, d);
          printf(" has less than TWO inputs!\n");
          exit(-1);
        }
        ActId **inputs = d->u.splitmerge.multi;
        for (int i = 0; i < numInputs; i++) {
          ActId *in = inputs[i];
          updateOpUses(in);
        }
        break;
      }
      case ACT_DFLOW_CLUSTER: {
        ActConnectVec actConnectVec;
        collectDflowClusterUses(d->u.dflow_cluster, actConnectVec);
        for (auto &actConnect: actConnectVec) {
          updateOpUses(actConnect);
        }
        break;
      }
      default: {
        printf("Unknown dataflow type %d\n", d->t);
        exit(-1);
      }
    }
  }
}

void ProcGenerator::createCopyProcs() {
  for (auto &opUsesIt: opUses) {
    unsigned uses = opUsesIt.second;
    if (uses) {
      unsigned numOut = uses + 1;
      act_connection *actConnection = opUsesIt.first;
      unsigned bitwidth = getBitwidth(actConnection);
      char *inName = new char[10240];
      getActConnectionName(actConnection, inName, 10240);
      double *metric = metrics->getOrGenCopyMetric(bitwidth, numOut);
      const char
          *instance = NameGenerator::genCopyInstName(bitwidth, numOut);
      chpBackend->createCopyProcs(instance, inName, metric);
    }
  }
}

void ProcGenerator::createSink(const char *name, unsigned bitwidth) {
  double *metric = metrics->getSinkMetric(bitwidth);
  const char *instance = NameGenerator::genSinkInstName(bitwidth);
  chpBackend->printSink(instance, name, metric);
}

void ProcGenerator::createSource(const char *outName,
                                 unsigned long val,
                                 unsigned bitwidth) {
  const char *instance = NameGenerator::genSourceInstName(val, bitwidth);
  double *metric = metrics->getSourceMetric(instance, bitwidth);
  chpBackend->printSource(instance, outName, metric);
}

void ProcGenerator::printDFlowFunc(DflowGenerator *dflowGenerator,
                                   const char *procName,
                                   UIntVec &outBWList,
                                   StringVec &outList,
                                   Map<unsigned int, unsigned int> &outRecord,
                                   Vector<BuffInfo> &buffInfos) {
  if (debug_verbose) {
    printf("PRINT DFLOW FUNCTION\n");
    printf("size: %d\n", strlen(procName));
    printf("procName: %s\n", procName);
    printf("outBWList:\n");
    for (auto &outWidth: outBWList) {
      printf("%u ", outWidth);
    }
    printf("\n");
    printf("outList:\n");
    for (auto &out: outList) {
      printf("%s ", out.c_str());
    }
    printf("\n");
    printf("buffInfos: ");
    for (auto &buffInfo: buffInfos) {
      printf("(%u, %lu, %lu, %d) ", buffInfo.outputID, buffInfo.nBuff,
             buffInfo.initVal, buffInfo.hasInitVal);
    }
    printf("\n");
    dflowGenerator->dump();
  }
  const char *calc = dflowGenerator->getCalc();
  StringVec &argList = dflowGenerator->getArgList();
  UIntVec &argBWList = dflowGenerator->getArgBWList();
  UIntVec &resBWList = dflowGenerator->getResBWList();
  Map<const char *, Expr *> &exprMap = dflowGenerator->getExprMap();
  StringMap<unsigned> &inBW = dflowGenerator->getInBW();
  StringMap<unsigned> &hiddenBW = dflowGenerator->getHiddenBWs();
  Map<Expr *, Expr *> &hiddenExprs = dflowGenerator->getHiddenExprs();

  char *instance = new char[MAX_INSTANCE_LEN];
  sprintf(instance, "%s<", procName);
  unsigned numArgs = argList.size();
  unsigned i = 0;
  for (; i < numArgs; i++) {
    char *subInstance = new char[100];
    if (i == (numArgs - 1)) {
      sprintf(subInstance, "%u>", argBWList[i]);
    } else {
      sprintf(subInstance, "%u,", argBWList[i]);
    }
    strcat(instance, subInstance);
  }
  double *fuMetric = metrics->getOrGenFUMetric(instance,
                                               inBW,
                                               hiddenBW,
                                               exprMap,
                                               hiddenExprs,
                                               outRecord,
                                               outBWList);
  chpBackend->printFU(procName,
                      instance,
                      argList,
                      argBWList,
                      resBWList,
                      outBWList,
                      calc,
                      outList,
                      outRecord,
                      buffInfos,
                      fuMetric);
  chpBackend->printBuff(buffInfos);
}

void ProcGenerator::handleDFlowFunc(DflowGenerator *dflowGenerator,
                                    act_dataflow_element *d,
                                    char *procName,
                                    int &resSuffix,
                                    StringVec &outList,
                                    UIntVec &outBWList,
                                    Map<unsigned int, unsigned int> &outRecord,
                                    Vector<BuffInfo> &buffInfos) {
  Expr *expr = d->u.func.lhs;
  int type = expr->type;
  ActId *rhs = d->u.func.rhs;
  char outName[10240];
  getActIdName(sc, rhs, outName, 10240);
  unsigned outBW = getActIdBW(rhs);
  Expr *initExpr = d->u.func.init;
  Expr *bufExpr = d->u.func.nbufs;
  if (debug_verbose) {
    printf("Handle expr ");
    print_expr(stdout, expr);
    printf("\n");
    if (bufExpr) {
      printf("[nbuf] ");
      rhs->Print(stdout);
      printf(", nBuff: ");
      print_expr(stdout, bufExpr);
      printf("\n");
      if (initExpr) {
        printf("initExpr: ");
        print_expr(stdout, initExpr);
        printf("\n");
      }
    }
  }
  if (type == E_INT) {
    unsigned long val = expr->u.v;
    createSource(outName, val, outBW);
    if (bufExpr) {
      print_expr(stdout, expr);
      printf(" has const lOp, but its rOp has buffer!\n");
      exit(-1);
    }
  } else {
    unsigned resBW = outBW;
    const char *exprName = printExpr(dflowGenerator,
                                     expr,
                                     procName,
                                     resSuffix,
                                     resBW);
    handlePort(expr,
               procName,
               resSuffix,
               resBW,
               exprName,
               dflowGenerator);
    outList.push_back(outName);
    outBWList.push_back(outBW);
    unsigned outID = outList.size() - 1;
    outRecord.insert({outID, resSuffix});

    if (bufExpr) {
      handleBuff(bufExpr, initExpr, procName, outName, outID, outBW, buffInfos);
    }
    {
      char outWidthStr[1024];
      sprintf(outWidthStr, "_%d", outBW);
      strcat(procName, outWidthStr);
    }
    if (debug_verbose) {
      printf("For dataflow element: ");
      dflow_print(stdout, d);
      printf("\n___________________________________________\n");
      printf("procName: %s\n", procName);
      printf("out bw: %d\n", outBW);
      printf("resSuffix: %d\n", resSuffix);
      printf("out: %s\n", outName);
      printf("init expr: ");
      print_expr(stdout, initExpr);
      printf("\n");
      dflowGenerator->dump();
    }
  }
}

void ProcGenerator::handleBuff(Expr *bufExpr,
                               Expr *initExpr,
                               char *procName,
                               const char *outName,
                               unsigned outID,
                               unsigned outBW,
                               Vector<BuffInfo> &buffInfos) {
  unsigned long numBuff = bufExpr->u.v;
  unsigned long initVal = -1;
  bool hasInitVal = false;
  if (initExpr) {
    if (initExpr->type != E_INT) {
      print_expr(stdout, initExpr);
      printf("The init value is not E_INT type!\n");
      exit(-1);
    }
    initVal = initExpr->u.v;
    hasInitVal = true;
    {
      char *subProcName = new char[1500];
      sprintf(subProcName, "_init%lu", initVal);
      strcat(procName, subProcName);
    }
  }
  double *buffMetric = metrics->getBuffMetric(numBuff, outBW);
  BuffInfo buff_info;
  buff_info.outputID = outID;
  buff_info.bw = outBW;
  buff_info.nBuff = numBuff;
  buff_info.initVal = initVal;
  buff_info.finalOutput = new char[1 + strlen(outName)];
  sprintf(buff_info.finalOutput, "%s", outName);
  buff_info.hasInitVal = hasInitVal;
  buff_info.metric = buffMetric;
  buffInfos.push_back(buff_info);
}

/* check if the expression only has E_VAR. Note that it could be built-in
 * int/bool, e.g., int(varName, bw). In this case, it still only has E_VAR expression. */
void ProcGenerator::handlePort(const Expr *expr,
                               char *procName,
                               int &resSuffix,
                               unsigned resBW,
                               const char *exprName,
                               DflowGenerator *dflowGenerator) {
  const Expr *actualExpr = expr;
  int type = expr->type;
  while ((type == E_BUILTIN_INT) || (type == E_BUILTIN_BOOL)) {
    actualExpr = actualExpr->u.e.l;
    type = actualExpr->type;
  }
  bool onlyVarExpr = (type == E_VAR);
  if (onlyVarExpr) {
    if (debug_verbose) {
      printf("The expression ");
      print_expr(stdout, expr);
      printf(" is port!\n");
    }
    {
      char *subProc = new char[10];
      if (procName[0] != '\0') {
        sprintf(subProc, "_port");
      } else {
        sprintf(subProc, "func_port");
      }
      strcat(procName, subProc);
    }
    resSuffix++;
    dflowGenerator->printChpPort(exprName, resSuffix, resBW);
    char *resName = new char[128];
    sprintf(resName, "res%d", resSuffix);
    dflowGenerator->preparePortForOpt(resName, exprName, resBW);
  }
}

void ProcGenerator::handleSelectionUnit(act_dataflow_element *d,
                                        CharPtrVec &inNameVec,
                                        char *&outputName,
                                        unsigned &dataBW,
                                        int &numInputs) {
  ActId *output = d->u.splitmerge.single;
  getActIdName(sc, output, outputName, 10240);
  const char *normalizedOutput = getNormActIdName(outputName);
  if (debug_verbose) {
    printf("[merge]: %s_inst\n", normalizedOutput);
  }
  dataBW = getActIdBW(output);
  numInputs = d->u.splitmerge.nmulti;
  ActId **inputs = d->u.splitmerge.multi;
  for (int i = 0; i < numInputs; i++) {
    ActId *in = inputs[i];
    const char *inStr = getActIdOrCopyName(in);
    inNameVec.push_back(inStr);
  }
}

void ProcGenerator::handleNormDflowElement(act_dataflow_element *d,
                                           unsigned &sinkCnt) {
  switch (d->t) {
    case ACT_DFLOW_FUNC: {
      if (debug_verbose) {
        printf("Process normal dflow:\n");
        dflow_print(stdout, d);
        printf("\n");
      }
      char *procName = new char[MAX_PROC_NAME_LEN];
      procName[0] = '\0';
      StringVec argList;
      StringVec oriArgList;
      UIntVec argBWList;
      UIntVec resBWList;
      int resSuffix = -1;
      StringVec outList;
      UIntVec outBWList;
      Vector<BuffInfo> buffInfos;
      Map<const char *, Expr *> exprMap;
      StringMap<unsigned> inBW;
      StringMap<unsigned> hiddenBW;
      Map<unsigned int, unsigned int> outRecord;
      Map<Expr *, Expr *> hiddenExprs;
      auto dflowGenerator = new DflowGenerator(argList,
                                               oriArgList,
                                               argBWList,
                                               resBWList,
                                               exprMap,
                                               inBW,
                                               hiddenBW,
                                               hiddenExprs);
      handleDFlowFunc(dflowGenerator,
                      d,
                      procName,
                      resSuffix,
                      outList,
                      outBWList,
                      outRecord,
                      buffInfos);
      const char *calc = dflowGenerator->getCalc();
      if (strlen(calc) > 1) {
        printDFlowFunc(dflowGenerator,
                       procName,
                       outBWList,
                       outList,
                       outRecord,
                       buffInfos);
      }
      break;
    }
    case ACT_DFLOW_SPLIT: {
      ActId *input = d->u.splitmerge.single;
      unsigned outBW = getActIdBW(input);
      ActId **outputs = d->u.splitmerge.multi;
      int numOutputs = d->u.splitmerge.nmulti;
      ActId *guard = d->u.splitmerge.guard;
      unsigned guardBW = getActIdBW(guard);
      char *splitName = new char[2000];
      char *inputName = new char[10240];
      getActIdName(sc, input, inputName, 10240);
      const char *normalizedInput = getNormActIdName(inputName);
      sprintf(splitName, "%s", normalizedInput);
      char *guardName = new char[10240];
      getActIdName(sc, guard, guardName, 10240);
      const char *normalizedGuard = getNormActIdName(guardName);
      strcat(splitName, normalizedGuard);
      CharPtrVec sinkVec;
      CharPtrVec outNameVec;
      for (int i = 0; i < numOutputs; i++) {
        ActId *out = outputs[i];
        if (!out) {
          strcat(splitName, "sink_");
          char *sinkName = new char[2100];
          sprintf(sinkName, "sink%d", sinkCnt);
          sinkCnt++;
          sinkVec.push_back(sinkName);
          outNameVec.push_back(sinkName);
        } else {
          char *outName = new char[10240];
          getActIdName(sc, out, outName, 10240);
          const char *normalizedOut = getNormActIdName(outName);
          strcat(splitName, normalizedOut);
          outNameVec.push_back(outName);
        }
      }
      for (auto &sink: sinkVec) {
        chpBackend->printChannel(sink, outBW);
        createSink(sink, outBW);
      }
      if (debug_verbose) {
        printf("[split]: %s\n", splitName);
      }
      const char *guardStr = getActIdOrCopyName(guard);
      const char *inputStr = getActIdOrCopyName(input);
      double *metric = metrics->getOrGenSplitMetric(guardBW,
                                                    outBW,
                                                    numOutputs);
      const char *instance =
          NameGenerator::genSplitInstName(guardBW, outBW, numOutputs);
      chpBackend->printSplit(instance,
                             splitName,
                             guardStr,
                             inputStr,
                             outNameVec,
                             numOutputs,
                             metric);
      break;
    }
    case ACT_DFLOW_MERGE: {
      CharPtrVec inNameVec;
      char *outputName = new char[10240];
      unsigned dataBW = 0;
      int numInputs = 0;
      handleSelectionUnit(d, inNameVec, outputName, dataBW, numInputs);
      ActId *ctrlIn = d->u.splitmerge.guard;
      unsigned ctrlBW = getActIdBW(ctrlIn);
      const char *ctrlInName = getActIdOrCopyName(ctrlIn);
      double *metric = metrics->getOrGenMergeMetric(ctrlBW,
                                                    dataBW,
                                                    numInputs);
      const char *instance =
          NameGenerator::genMergeInstName(ctrlBW, dataBW, numInputs);
      chpBackend->printMerge(instance,
                             outputName,
                             ctrlInName,
                             inNameVec,
                             metric);
      break;
    }
    case ACT_DFLOW_MIXER: {
      CharPtrVec inNameVec;
      char *outputName = new char[10240];
      unsigned dataBW = 0;
      int numInputs = 0;
      handleSelectionUnit(d, inNameVec, outputName, dataBW, numInputs);
      double *metric = metrics->getMixerMetric(numInputs, dataBW);
      const char *instance = NameGenerator::genMixerInstName(dataBW, numInputs);
      chpBackend->printMixer(instance, outputName, inNameVec, metric);
      break;
    }
    case ACT_DFLOW_ARBITER: {
      CharPtrVec inNameVec;
      char *outputName = new char[10240];
      unsigned dataBW = 0;
      int numInputs = 0;
      handleSelectionUnit(d, inNameVec, outputName, dataBW, numInputs);
      ActId *ctrlOut = d->u.splitmerge.nondetctrl;
      unsigned ctrlBW = getActIdBW(ctrlOut);
      char *ctrlOutName = new char[10240];
      getActIdName(sc, ctrlOut, ctrlOutName, 10240);
      const char *instance =
          NameGenerator::genArbiterInstName(ctrlBW, dataBW, numInputs);
      double *metric = metrics->getArbiterMetric(
          numInputs,
          dataBW,
          ctrlBW);
      chpBackend->printArbiter(
          instance,
          outputName,
          ctrlOutName,
          inNameVec,
          metric);
      break;
    }
    case ACT_DFLOW_CLUSTER: {
      dflow_print(stdout, d);
      printf("We should not process dflow_clsuter here!");
      exit(-1);
    }
    case ACT_DFLOW_SINK: {
      ActId *input = d->u.sink.chan;
      char *inputName = new char[10240];
      getActIdName(sc, input, inputName, 10240);
      unsigned bw = getBitwidth(input->Canonical(sc));
      createSink(inputName, bw);
      if (debug_verbose) {
        printf("%s is not used anywhere!\n", inputName);
      }
      break;
    }
    default: {
      printf("Unknown dataflow type %d\n", d->t);
      exit(-1);
    }
  }
}

void ProcGenerator::handleDFlowCluster(list_t *dflow) {
  char *procName = new char[MAX_CLUSTER_PROC_NAME_LEN];
  procName[0] = '\0';
  IntVec boolResSuffixs;
  char *def = new char[10240];
  def[0] = '\0';
  sprintf(def, "\n");
  StringVec argList;
  StringVec oriArgList;
  UIntVec argBWList;
  UIntVec resBWList;
  int resSuffix = -1;
  StringVec outList;
  UIntVec outBWList;
  Vector<BuffInfo> buffInfos;
  Map<const char *, Expr *> exprMap;
  StringMap<unsigned> inBW;
  StringMap<unsigned> hiddenBW;
  Map<unsigned int, unsigned int> outRecord;
  Map<Expr *, Expr *> hiddenExprs;
  unsigned elementCnt = 0;
  auto dflowGenerator = new DflowGenerator(argList,
                                           oriArgList,
                                           argBWList,
                                           resBWList,
                                           exprMap,
                                           inBW,
                                           hiddenBW,
                                           hiddenExprs);
  listitem_t *li;
  for (li = list_first (dflow); li; li = list_next (li)) {
    auto *d = (act_dataflow_element *) list_value (li);
    if (debug_verbose) {
      printf("Start to process dflow_cluster element ");
      dflow_print(stdout, d);
      printf(", current proc name: %s\n", procName);
    }
    if (d->t == ACT_DFLOW_FUNC) {
      handleDFlowFunc(dflowGenerator,
                      d,
                      procName,
                      resSuffix,
                      outList,
                      outBWList,
                      outRecord,
                      buffInfos);
      char *subProc = new char[1024];
      sprintf(subProc, "_p%d", elementCnt);
      elementCnt++;
      strcat(procName, subProc);
    } else {
      dflow_print(stdout, d);
      printf("This dflow statement should not appear in dflow-cluster!\n");
      exit(-1);
    }
    if (debug_verbose) {
      printf("After processing dflow_cluster element ");
      dflow_print(stdout, d);
      printf(", the proc name: %s\n", procName);
    }
  }
  if (debug_verbose) {
    printf("Process cluster dflow:\n");
    print_dflow(stdout, dflow);
    printf("\n");
  }
  const char *calc = dflowGenerator->getCalc();
  if (strlen(calc) > 1) {
    printDFlowFunc(dflowGenerator,
                   procName,
                   outBWList,
                   outList,
                   outRecord,
                   buffInfos);
  }
}

ProcGenerator::ProcGenerator(Metrics *metrics,
                             ChpBackend *chpBackend,
                             Process *p) {
  this->metrics = metrics;
  this->chpBackend = chpBackend;
  this->p = p;
  this->sc = p->CurScope();
}

void ProcGenerator::run() {
  const char *pName = p->getName();
  if (debug_verbose) {
    printf("processing %s\n", pName);
  }
  if (p->getlang()->getchp()) {
    chpBackend->createChpBlock(p);
    return;
  }
  if (!p->getlang()->getdflow()) {
    printf("Process `%s': no dataflow body", p->getName());
    exit(-1);
  }
  chpBackend->printProcHeader(p);
  collectBitwidthInfo();
  collectOpUses();
  createCopyProcs();
  listitem_t *li;
  unsigned sinkCnt = 0;
  for (li = list_first (p->getlang()->getdflow()->dflow); li;
       li = list_next (li)) {
    auto *d = (act_dataflow_element *) list_value (li);
    if (d->t == ACT_DFLOW_CLUSTER) {
      list_t *dflow_cluster = d->u.dflow_cluster;
      handleDFlowCluster(dflow_cluster);
    } else {
      handleNormDflowElement(d, sinkCnt);
    }
  }
  chpBackend->printProcEnding();
}
