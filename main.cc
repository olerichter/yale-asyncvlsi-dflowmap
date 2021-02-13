/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2020 Rajit Manohar
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *
 **************************************************************************
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <act/act.h>
#include <act/passes.h>
#include "lib.cc"

FILE *libFp;
FILE *resFp;

std::map<const char *, int> bitwidthMap;
/* operator, # of times it is used (if it is used for more than once, then we create COPY for it) */
std::map<const char *, unsigned> opUses;
/* copy operator, # of times it has already been used */
std::map<const char *, unsigned> copyUses;

int getBitwidth(const char *varName);

unsigned getOpUses(const char *op);

unsigned getCopyUses(const char *copyOp);

static void usage(char *name) {
  fprintf(stderr, "Usage: %s <actfile>\n", name);
  exit(1);
}

void printActId(ActId *actId) {
  if (actId) {
    const char *actName = actId->getName();
    unsigned outUses = getOpUses(actName);
    if (outUses) {
      unsigned copyUse = getCopyUses(actName);
      if (copyUse <= outUses) {
        fprintf(resFp, "%scopy.out[%u]", actId->getName(), copyUse);
      } else {
        fprintf(resFp, "%s", actId->getName());
      }
    } else {
      fprintf(resFp, "%s", actId->getName());
    }
    fprintf(resFp, ", ");
  } else {
    fprintf(resFp, "*, ");
  }
}

int getExprBitwidth(Expr *expr) {
  int type = expr->type;
  if (type == E_VAR) {
    auto actId = (ActId *) expr->u.e.l;
    return getBitwidth(actId->getName());
  } else if (type == E_INT) {
    return -1;
  } else {
    printf("Try to get bitwidth for invalid expr type: %d!\n", type);
    print_expr(stdout, expr);
    exit(-1);
  }
}

void printInt(const char *out, unsigned intVal, int bitwidth) {
  fprintf(resFp, "source<%u, %d> %ssource_inst(%ssource_);\n", intVal, bitwidth, out, out);
  createSource(libFp);
  fprintf(resFp, "%ssource_, ", out);
}

void printExpr(Expr *expr, const char *out, int bitwidth) {
  int type = expr->type;
  if (type == E_VAR) {
    auto actId = (ActId *) expr->u.e.l;
    printActId(actId);
  } else if (type == E_INT) {
    unsigned intVal = expr->u.v;
    fprintf(resFp, "%u, ", intVal);
  } else {
    fprintf(resFp, "Try to get name for invalid expr type: %d!\n", type);
    print_expr(stdout, expr);
    exit(-1);
  }
}

void collectBitwidthInfo(Process *p) {
  ActInstiter inst(p->CurScope());
  for (inst = inst.begin(); inst != inst.end(); inst++) {
    ValueIdx *vx = *inst;
    const char *varName = vx->getName();
    int bitwidth = TypeFactory::bitWidth(vx->t);
    bitwidthMap.insert(std::make_pair(varName, bitwidth));
  }
}

void printBitwidthInfo() {
  printf("bitwidth info:\n");
  for (auto bitwidthMapIt = bitwidthMap.begin(); bitwidthMapIt != bitwidthMap.end();
       bitwidthMapIt++) {
    printf("(%s, %d) ", bitwidthMapIt->first, bitwidthMapIt->second);
  }
  printf("\n");
}

int getBitwidth(const char *varName) {
  for (auto &bitwidthMapIt : bitwidthMap) {
    const char *record = bitwidthMapIt.first;
    if (strcmp(record, varName) == 0) {
      return bitwidthMapIt.second;
    }
  }
  printf("We could not find bitwidth info for %s\n", varName);
  printBitwidthInfo();
  exit(-1);
}

void EMIT_BIN(Expr *expr, const char *out, const char *sym, int outWidth) {
  /* collect bitwidth info */
  Expr *lExpr = expr->u.e.l;
  int lWidth = getExprBitwidth(lExpr);
  Expr *rExpr = expr->u.e.r;
  int rWidth = getExprBitwidth(rExpr);
  int inWidth;
  if (lWidth != -1) {
    inWidth = lWidth;
  } else if (rWidth != -1) {
    inWidth = rWidth;
  } else {
    printf("Expression has constants for both operands, which should be optimized away by "
           "compiler!\n");
    print_expr(stdout, expr);
    exit(-1);
  }
  /* print */
  fprintf(resFp, "func_%s<%d, %d> %s_inst(", sym, inWidth, outWidth, out);
  printExpr(lExpr, out, inWidth);
  printExpr(rExpr, out, inWidth);
  fprintf(resFp, "%s);\n", out);
}

void EMIT_UNI(Expr *expr, const char *out, const char *sym) {
  /* collect bitwidth info */
  Expr *lExpr = expr->u.e.l;
  int lWidth = getExprBitwidth(lExpr);
  if (lWidth == -1) {
    printf("UniExpr has constant for input, which should be optimized by compiler!\n");
    print_expr(stdout, expr);
    exit(-1);
  }
  fprintf(resFp, "func_%s<%d> %s_uniinst(", sym, lWidth, out);
  printExpr(lExpr, out, lWidth);
  fprintf(resFp, "%s);\n", out);
}

unsigned getCopyUses(const char *op) {
  auto copyUsesIt = copyUses.find(op);
  if (copyUsesIt == copyUses.end()) {
    printf("We don't know how many times %s is used as COPY!\n", op);
    exit(-1);
  }
  unsigned uses = copyUsesIt->second;
  copyUsesIt->second++;
  return uses;
}

void updateOpUses(const char *op) {
  auto opUsesIt = opUses.find(op);
  if (opUsesIt == opUses.end()) {
    opUses.insert(std::make_pair(op, 0));
    copyUses.insert(std::make_pair(op, 0));
  } else {
    opUsesIt->second++;
  }
}

void printOpUses() {
  printf("OP USES:\n");
  for (auto &opUsesIt : opUses) {
    printf("(%s, %u) ", opUsesIt.first, opUsesIt.second);
  }
  printf("\n");
}

unsigned getOpUses(const char *op) {
  auto opUsesIt = opUses.find(op);
  if (opUsesIt == opUses.end()) {
    printf("We don't know how many times %s is used!\n", op);
    printOpUses();
    return 0;
//    exit(-1);
  }
  return opUsesIt->second;
}

void collectUniOpUses(Expr *expr) {
  Expr *lExpr = expr->u.e.l;
  if (lExpr->type == E_VAR) {
    auto actId = (ActId *) lExpr->u.e.l;
    updateOpUses(actId->getName());
  }
}

void collectBinOpUses(Expr *expr) {
  Expr *lExpr = expr->u.e.l;
  if (lExpr->type == E_VAR) {
    auto actId = (ActId *) lExpr->u.e.l;
    updateOpUses(actId->getName());
  }
  Expr *rExpr = expr->u.e.r;
  if (rExpr->type == E_VAR) {
    auto actId = (ActId *) rExpr->u.e.l;
    updateOpUses(actId->getName());
  }
}

void collectOpUses(Process *p) {
  listitem_t *li;
  for (li = list_first (p->getlang()->getdflow()->dflow);
       li;
       li = list_next (li)) {
    auto *d = (act_dataflow_element *) list_value (li);
    switch (d->t) {
      case ACT_DFLOW_FUNC: {
        Expr *expr = d->u.func.lhs;
        ActId *rhs = d->u.func.rhs;
        if (!rhs) {
          fprintf(stdout, "dflow function has empty RHS!\n");
          print_expr(stdout, expr);
          exit(-1);
        }
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
            collectBinOpUses(expr);
            break;
          }
          case E_NOT:
          case E_UMINUS:
          case E_COMPLEMENT: {
            collectUniOpUses(expr);
            break;
          }
          case E_INT: {
            break;
          }
          case E_VAR: {
            auto actId = (ActId *) expr->u.e.l;
            updateOpUses(actId->getName());
            break;
          }
          default: {
            fatal_error("Unknown expression type %d\n", type);
          }
        }
        break;
      }
      case ACT_DFLOW_SPLIT: {
        ActId *input = d->u.splitmerge.single;
        updateOpUses(input->getName());
        ActId *guard = d->u.splitmerge.guard;
        updateOpUses(guard->getName());
        break;
      }
      case ACT_DFLOW_MERGE: {
        ActId *guard = d->u.splitmerge.guard;
        updateOpUses(guard->getName());
        int numInputs = d->u.splitmerge.nmulti;
        if (numInputs != 2) {
          fatal_error("Merge does not have TWO outputs!\n");
        }
        ActId **inputs = d->u.splitmerge.multi;
        ActId *lIn = inputs[0];
        updateOpUses(lIn->getName());
        ActId *rIn = inputs[1];
        updateOpUses(rIn->getName());
        break;
      }
      case ACT_DFLOW_MIXER: {
        fatal_error("We don't support MIXER for now!\n");
        break;
      }
      case ACT_DFLOW_ARBITER: {
        fatal_error("We don't support ARBITER for now!\n");
        break;
      }
      default: {
        fatal_error("Unknown dataflow type %d\n", d->t);
        break;
      }
    }
  }
}

void createCopyProcs() {
  fprintf(resFp, "/* copy processes */\n");
  for (auto &opUsesIt : opUses) {
    unsigned uses = opUsesIt.second;
    if (uses) {
      const char *opName = opUsesIt.first;
      int bitwidth = getBitwidth(opName);
      createCopy(libFp);
      fprintf(resFp, "copy<%d, %u> %scopy(%s);\n", bitwidth, uses + 1, opName, opName);
    }
  }
  fprintf(resFp, "\n");
}

void handleProcess(Process *p) {
  const char *procName = p->getName();
  fprintf(stdout, "processing %s\n", procName);
  bool mainProc = (strcmp(procName, "main<>") == 0);
  if (!p->getlang()->getdflow()) {
    fatal_error("Process `%s': no dataflow body", p->getName());
  }
  p->PrintHeader(resFp, "defproc");
  fprintf(resFp, "\n{");
  p->CurScope()->Print(resFp);
  bitwidthMap.clear();
  opUses.clear();
  copyUses.clear();
  collectBitwidthInfo(p);
  collectOpUses(p);
  createCopyProcs();
  listitem_t *li;
  for (li = list_first (p->getlang()->getdflow()->dflow);
       li;
       li = list_next (li)) {
    auto *d = (act_dataflow_element *) list_value (li);
    switch (d->t) {
      case ACT_DFLOW_FUNC: {
        /* handle left hand side */
        Expr *expr = d->u.func.lhs;
        int type = expr->type;
        /* handle right hand side */
        ActId *rhs = d->u.func.rhs;
        const char *out = rhs->getName();
        int outWidth = getBitwidth(out);
        Expr *init = d->u.func.init;
        if (init) {
          if (type != E_VAR) {
            printf("We have init in ACT code, but the left side is not E_VAR!\n");
            print_expr(stdout, expr);
            printf(" => ");
            printActId(rhs);
            printf("\n");
            exit(-1);
          }
        }
        switch (type) {
          case E_AND: {
            EMIT_BIN(expr, out, "and", outWidth);
            createBinLib(libFp, "and", "&", type);
            break;
          }
          case E_OR: {
            EMIT_BIN(expr, out, "or", outWidth);
            createBinLib(libFp, "or", "|", type);
            break;
          }
          case E_NOT: {
            EMIT_UNI(expr, out, "not");
            createUniLib(libFp, "not", "~", type);
            break;
          }
          case E_PLUS: {
            EMIT_BIN(expr, out, "add", outWidth);
            createBinLib(libFp, "add", "+", type);
            break;
          }
          case E_MINUS: {
            EMIT_BIN(expr, out, "minus", outWidth);
            createBinLib(libFp, "minus", "-", type);
            break;
          }
          case E_MULT: {
            EMIT_BIN(expr, out, "multi", outWidth);
            createBinLib(libFp, "multi", "*", type);
            break;
          }
          case E_DIV: {
            EMIT_BIN(expr, out, "div", outWidth);
            createBinLib(libFp, "div", "/", type);
            break;
          }
          case E_MOD: {
            EMIT_BIN(expr, out, "mod", outWidth);
            createBinLib(libFp, "mod", "%", type);
            break;
          }
          case E_LSL: {
            EMIT_BIN(expr, out, "lsl", outWidth);
            createBinLib(libFp, "lsl", "<<", type);
            break;
          }
          case E_LSR: {
            EMIT_BIN(expr, out, "lsr", outWidth);
            createBinLib(libFp, "lsr", ">>", type);
            break;
          }
          case E_ASR: {
            EMIT_BIN(expr, out, "asr", outWidth);
            createBinLib(libFp, "asr", ">>>", type);
            break;
          }
          case E_UMINUS: {
            EMIT_UNI(expr, out, "neg");
            createUniLib(libFp, "neg", "-", type);
            break;
          }
          case E_INT: {
            unsigned int val = expr->u.v;
            fprintf(resFp, "source<%u, %d> %s_inst(%s);\n", val, outWidth, out, out);
            createSource(libFp);
            break;
          }
          case E_VAR: {
            unsigned initVal = 0;
            auto actId = (ActId *) expr->u.e.l;
            if (init) {
              int initValType = init->type;
              if (initValType != E_INT) {
                print_expr(stdout, init);
                printf("The init value is not E_INT type!\n");
                exit(-1);
              }
              initVal = init->u.v;
              fprintf(resFp, "init<%u, %d> %s_inst(", initVal, outWidth, out);
              printActId(actId);
              createInit(libFp);
            } else {
              fprintf(resFp, "buffer<%d> %s_inst(", outWidth, out);
              printActId(actId);
              createBuff(libFp);
            }
            fprintf(resFp, "%s);\n", out);
            break;
          }
          case E_XOR: {
            EMIT_BIN(expr, out, "xor", outWidth);
            createBinLib(libFp, "xor", "^", type);
            break;
          }
          case E_LT: {
            EMIT_BIN(expr, out, "lt", outWidth);
            createBinLib(libFp, "lt", "<", type);
            break;
          }
          case E_GT: {
            EMIT_BIN(expr, out, "gt", outWidth);
            createBinLib(libFp, "gt", ">", type);
            break;
          }
          case E_LE: {
            EMIT_BIN(expr, out, "le", outWidth);
            createBinLib(libFp, "le", "<=", type);
            break;
          }
          case E_GE: {
            EMIT_BIN(expr, out, "ge", outWidth);
            createBinLib(libFp, "ge", ">=", type);
            break;
          }
          case E_EQ: {
            EMIT_BIN(expr, out, "eq", outWidth);
            createBinLib(libFp, "eq", "=", type);
            break;
          }
          case E_NE: {
            EMIT_BIN(expr, out, "ne", outWidth);
            createBinLib(libFp, "ne", "!=", type);
            break;
          }
          case E_COMPLEMENT: {
            EMIT_UNI(expr, out, "comple");
            createUniLib(libFp, "comple", "~", type);
            break;
          }
          default: {
            fatal_error("Unknown expression type %d\n", type);
          }
        }
//        checkAndCreateCopy(libFp, resFp, out, outWidth);
        break;
      }
      case ACT_DFLOW_SPLIT: {
        ActId *input = d->u.splitmerge.single;
        const char* inputName = input->getName();
        size_t  inputSize = strlen(inputName);
        int bitwidth = getBitwidth(inputName);
        ActId **outputs = d->u.splitmerge.multi;
        ActId *lOut = outputs[0];
        ActId *rOut = outputs[1];
        const char *outName = nullptr;
        char *splitName = nullptr;
        if (!lOut && !rOut) {  // split has empty target for both ports
          splitName = new char[inputSize + 7];
          strcpy(splitName, inputName);
          strcat(splitName, "_SPLIT");
        } else {
          if (lOut) {
            outName = lOut->getName();
          } else {
            outName = rOut->getName();
          }
          size_t splitSize = strlen(outName) - 1;
          splitName = new char[splitSize];
          memcpy(splitName, outName, splitSize - 1);
          splitName[splitSize - 1] = '\0';
        }
        if (!lOut) {
          fprintf(resFp, "sink<%d> %s_L_sink(%s_L);\n", bitwidth, splitName, splitName);
        }
        if (!rOut) {
          fprintf(resFp, "sink<%d> %s_R_sink(%s_R);\n", bitwidth, splitName, splitName);
        }
        fprintf(resFp, "control_split<%d> %s_inst(", bitwidth, splitName);
        ActId *guard = d->u.splitmerge.guard;
        printActId(guard);
        printActId(input);
        fprintf(resFp, "%s_L, %s_R);\n", splitName, splitName);
        createSplit(libFp);
        break;
      }
      case ACT_DFLOW_MERGE: {
        ActId *output = d->u.splitmerge.single;
        const char *outputName = output->getName();
        int bitwidth = getBitwidth(outputName);
        fprintf(resFp, "control_merge<%d> %s_inst(", bitwidth, outputName);
        ActId *guard = d->u.splitmerge.guard;
        printActId(guard);
        ActId **inputs = d->u.splitmerge.multi;
        ActId *lIn = inputs[0];
        printActId(lIn);
        ActId *rIn = inputs[1];
        printActId(rIn);
        fprintf(resFp, "%s);\n", outputName);
        createMerge(libFp);
        break;
      }
      case ACT_DFLOW_MIXER: {
        fatal_error("We don't support MIXER for now!\n");
        break;
      }
      case ACT_DFLOW_ARBITER: {
        fatal_error("We don't support ARBITER for now!\n");
        break;
      }
      default: {
        fatal_error("Unknown dataflow type %d\n", d->t);
        break;
      }
    }
  }
  if (mainProc) {
    const char *outPort = "main_out";
    int outWidth = getBitwidth(outPort);
    fprintf(resFp, "sink<%d> main_out_sink(main_out);\n", outWidth);
    createSink(libFp);
  }
  fprintf(resFp, "}\n\n");
}

void initialize() {
  for (unsigned i = 0; i < MAX_EXPR_TYPE_NUM; i++) {
    fuIDs[i] = -1;
  }
  for (unsigned i = 0; i < MAX_PROCESSES; i++) {
    processes[i] = nullptr;
  }
}

int main(int argc, char **argv) {
  /* initialize ACT library */
  Act::Init(&argc, &argv);

  /* some usage check */
  if (argc != 2) {
    usage(argv[0]);
  }

  /* read in the ACT file */
  Act *a = new Act(argv[1]);
  a->Expand();
  a->mangle(NULL);
  fprintf(stdout, "Processing ACT file %s!\n", argv[1]);
  char *result_file = new char[8 + strlen(argv[1])];
  strcpy(result_file, "result_");
  strcat(result_file, argv[1]);
  resFp = fopen(result_file, "w");
  char *lib_file = new char[5 + strlen(argv[1])];
  strcpy(lib_file, "lib_");
  strcat(lib_file, argv[1]);
  libFp = fopen(lib_file, "w");
  fprintf(resFp, "import \"%s\";\n\n", lib_file);
  ActTypeiter it(a->Global());
  initialize();
  Process *procArray[MAX_PROCESSES];
  unsigned index = 0;
  for (it = it.begin(); it != it.end(); it++) {
    Type *t = *it;
    auto p = dynamic_cast<Process *>(t);
    if (p->isExpanded()) {
      procArray[index] = p;
      index++;
    }
  }
  for (int i = index - 1; i >= 0; i--) {
    Process *p = procArray[i];
    handleProcess(p);
  }
  fprintf(resFp, "main m;\n");
  fclose(resFp);
  fclose(libFp);
  return 0;
}

