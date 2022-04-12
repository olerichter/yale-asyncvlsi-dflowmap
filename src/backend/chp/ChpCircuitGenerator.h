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

#ifndef DFLOWMAP__CHPCIRCUITGENERATOR_H_
#define DFLOWMAP__CHPCIRCUITGENERATOR_H_

#include <act/act.h>
#include <act/iter.h>
#include <fstream>
#include "src/common/Helper.h"

class ChpCircuitGenerator {
 private:
  FILE *chpFp;
  FILE *netlistFp;

 public:
  explicit ChpCircuitGenerator(FILE *chpFp, FILE* netlistFp);

  void printSink(const char *instance, const char *inName);

  void printCopy(const char *instance,
                 const char *inputName);

  void printEmptyLine();

  void printInit(const char *instance,
                 const char *inName,
                 const char *outName);

  void printOneBuff(const char *instance,
                    const char *inName,
                    const char *outName);

  void printBuff(Vector<BuffInfo> &buffInfos);

  void printChannel(const char* chanName, unsigned bitwidth);

  void printSource(const char *instance, const char *outName);

  void printFunc(const char *instance,
                 StringVec &argList,
                 StringVec &outList,
                 Vector<BuffInfo> &buffInfos);

  void printSplit(const char *instance,
                  const char *splitName,
                  const char *guardName,
                  const char *inputName,
                  CharPtrVec &outNameVec);

  void printMerge(const char *instance,
                  const char *outName,
                  const char *guardStr,
                  CharPtrVec &inNameVec);

  void printArbiter(const char *instance,
                    const char *outName,
                    const char *coutName,
                    CharPtrVec &inNameVec);

  void printMixer(const char *instance,
                  const char *outName,
                  CharPtrVec &inNameVec);

  void printProcHeader(Process *p);

  void printProcDeclaration(Process *p);

  void printProcEnding();

  void printCustomNamespace(ActNamespace *ns);

  void printFileEnding();
};

#endif //DFLOWMAP__CHPCIRCUITGENERATOR_H_
