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

#include "NameGenerator.h"

const char *NameGenerator::genMergeInstName(unsigned guardBW,
                                            unsigned inBW,
                                            int numInputs) {
  char *procName = new char[MAX_PROC_NAME_LEN];
  if (PIPELINE) {
    sprintf(procName,
            "dflowstd::pipe_%s%d",
            Constant::MERGE_PREFIX,
            numInputs);
  } else {
    sprintf(procName,
            "dflowstd::unpipe_%s%d",
            Constant::MERGE_PREFIX,
            numInputs);
  }
  char *instance = new char[MAX_INSTANCE_LEN];
  sprintf(instance, "%s<%d,%d>", procName, guardBW, inBW);
  return instance;
}

const char *NameGenerator::genMixerInstName(unsigned inBW, int numInputs) {
  char *procName = new char[MAX_PROC_NAME_LEN];
  if (PIPELINE) {
    sprintf(procName,
            "dflowstd::pipe_%s%d",
            Constant::MIXER_PREFIX,
            numInputs);
  } else {
    sprintf(procName,
            "dflowstd::unpipe_%s%d",
            Constant::MIXER_PREFIX,
            numInputs);
  }
  char *instance = new char[MAX_INSTANCE_LEN];
  sprintf(instance, "%s<%d>", procName, inBW);
  return instance;
}

const char *NameGenerator::genArbiterInstName(unsigned guardBW,
                                              unsigned inBW,
                                              int numInputs) {
  char *procName = new char[MAX_PROC_NAME_LEN];
  if (PIPELINE) {
    sprintf(procName,
            "dflowstd::pipe_%s%d",
            Constant::ARBITER_PREFIX,
            numInputs);
  } else {
    sprintf(procName,
            "dflowstd::unpipe_%s%d",
            Constant::ARBITER_PREFIX,
            numInputs);
  }
  char *instance = new char[MAX_INSTANCE_LEN];
  sprintf(instance, "%s<%d,%d>", procName, inBW, guardBW);
  return instance;
}

const char *NameGenerator::genSplitInstName(unsigned guardBW,
                                            unsigned outBW,
                                            int numOut) {
  char *procName = new char[MAX_PROC_NAME_LEN];
  if (PIPELINE) {
    sprintf(procName, "dflowstd::pipe_%s%d", Constant::SPLIT_PREFIX, numOut);
  } else {
    sprintf(procName,
            "dflowstd::unpipe_%s%d",
            Constant::SPLIT_PREFIX,
            numOut);
  }
  char *instance = new char[MAX_INSTANCE_LEN];
  sprintf(instance, "%s<%d,%d>", procName, guardBW, outBW);
  return instance;
}

const char *NameGenerator::genCopyInstName(unsigned bw, unsigned numOut) {
  char *procName = new char[1024];
  sprintf(procName, "dflowstd::copy");
  char *instance = new char[1024];
  sprintf(instance, "%s<%u,%u>", procName, bw, numOut);
  return instance;
}

const char *NameGenerator::genSinkInstName(unsigned bw) {
  char *instance = new char[1500];
  sprintf(instance, "dflowstd::sink<%u>", bw);
  return instance;
}

const char *NameGenerator::genSourceInstName(unsigned long val,
                                             unsigned bitwidth) {
  char *instance = new char[1500];
  sprintf(instance, "dflowstd::source<%lu,%u>", val, bitwidth);
  return instance;
}

const char *NameGenerator::genExprName(Expr *expr) {
  list_t *arg_list = list_new();
  act_expr_collect_ids(arg_list, expr);
  return act_expr_to_string(arg_list, expr);
}

const char *NameGenerator::genExprClusterName(list_t *dflow_cluster) {
  Vector<Expr *> exprList;
  for (listitem_t *li = list_first (dflow_cluster); li; li = list_next (li)) {
    auto *d = (act_dataflow_element *) list_value (li);
    if (d->t != ACT_DFLOW_FUNC) {
      dflow_print(stdout, d);
      printf("\nThis dflow statement should not appear in dflow-cluster!\n");
      exit(-1);
    }
    Expr *expr = d->u.func.lhs;
    exprList.push_back(expr);
  }
  list_t *argList = list_new();
  for (auto &e: exprList) {
    act_expr_collect_ids(argList, e);
  }
  char *name = new char[MAX_CLUSTER_PROC_NAME_LEN];
  name[0] = '\0';
  char *delimiter = new char[1];
  sprintf(delimiter, "_");
  for (auto &e: exprList) {
    if (name[0] != '\0') strcat(name, delimiter);
    strcat(name, act_expr_to_string(argList, e));
  }
  return name;
}

const char *NameGenerator::genFUName(const char *procName,
                                     StringVec &argList,
                                     UIntVec &outBWList,
                                     UIntVec &argBWList) {
  char *instance = new char[MAX_INSTANCE_LEN];
  sprintf(instance, "%s<", procName);
  unsigned numArgs = argList.size();
  unsigned numOuts = outBWList.size();
  for (int i = 0; i < numArgs; i++) {
    char *subInstance = new char[100];
    sprintf(subInstance, "%u,", argBWList[i]);
    strcat(instance, subInstance);
  }
  for (int i = 0; i < numOuts; i++) {
    char *subInstance = new char[100];
    if (i == (numOuts - 1)) {
      sprintf(subInstance, "%u>", outBWList[i]);
    } else {
      sprintf(subInstance, "%u,", outBWList[i]);
    }
    strcat(instance, subInstance);
  }
  return instance;
}