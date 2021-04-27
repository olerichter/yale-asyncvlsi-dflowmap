#include "ChpProcGenerator.h"

void ChpProcGenerator::initialize() {
  for (unsigned i = 0; i < MAX_PROCESSES; i++) {
    processes[i] = nullptr;
  }
  for (unsigned i = 0; i < MAX_PROCESSES; i++) {
    instances[i] = nullptr;
  }
}

bool ChpProcGenerator::hasInstance(const char *instance) {
  for (unsigned i = 0; i < MAX_PROCESSES; i++) {
    if (instances[i] == nullptr) {
      instances[i] = instance;
      return false;
    } else if (!strcmp(instances[i], instance)) {
      return true;
    }
  }
  return false;
}

bool ChpProcGenerator::hasProcess(const char *process) {
  for (unsigned i = 0; i < MAX_PROCESSES; i++) {
    if (processes[i] == nullptr) {
      processes[i] = process;
      return false;
    } else if (!strcmp(processes[i], process)) {
      return true;
    }
  }
  return false;
}

void ChpProcGenerator::createFULib(FILE *libFp, FILE *confFp, const char *procName, const char *calc, const char *def,
                                   const char *outSend, const char *initSend, int numArgs, int numOuts, int
                             numRes,
                                   const char *instance, int *metric, IntVec &boolRes) {
  if (!hasProcess(procName)) {
    fprintf(libFp, "template<pint ");
    int i = 0;
    for (; i < (numArgs + numOuts + numRes - 1); i++) {
      fprintf(libFp, "W%d, ", i);
    }
    fprintf(libFp, "W%d>\n", i);
    fprintf(libFp, "defproc %s(", procName);
    for (i = 0; i < numArgs; i++) {
      fprintf(libFp, "chan?(int<W%d>)arg%d; ", i, i);
    }
    for (i = 0; i < numOuts - 1; i++) {
      fprintf(libFp, "chan!(int<W%d>) out%d; ", i + numArgs, i);
    }
    fprintf(libFp, "chan!(int<W%d>) out%d) {\n", i + numArgs, i);

    for (i = 0; i < numArgs; i++) {
      fprintf(libFp, "  int<W%d> x%d;\n", i, i);
    }
    for (i = 0; i < numRes; i++) {
      if (std::find(boolRes.begin(), boolRes.end(), i) == boolRes.end()) {
        fprintf(libFp, "  int<W%d> res%d;\n", i + numArgs + numOuts, i);
      } else {
        fprintf(libFp, "  bool res%d;\n", i);
      }
    }
    fprintf(libFp, "%s", def);
    fprintf(libFp, "  chp {\n");
    if (initSend) {
      fprintf(libFp, "%s", initSend);
    }
    fprintf(libFp, "    *[\n");

    if (DEBUG_FU) {
      for (i = 0; i < numArgs - 1; i++) {
        fprintf(libFp, "      arg%d?x%d;  log(\"receive (\", x%d, \")\");\n", i, i, i);
      }
      fprintf(libFp, "      arg%d?x%d;  log(\"receive (\", x%d, \")\");\n", i, i, i);
    } else {
      for (i = 0; i < numArgs - 1; i++) {
        fprintf(libFp, "arg%d?x%d, ", i, i);
      }
      fprintf(libFp, "arg%d?x%d;\n", i, i);
      fprintf(libFp, "      log(\"receive (\", ");
      for (i = 0; i < numArgs - 1; i++) {
        fprintf(libFp, "x%d, \",\", ", i);
      }
      fprintf(libFp, "x%d, \")\");\n", i);
    }
    fprintf(libFp, "%s", calc);
    fprintf(libFp, "%s", outSend);
    fprintf(libFp, "\n    ]\n  }\n}\n\n");
  }
  if (!hasInstance(instance)) {
    fprintf(confFp, "begin %s\n", instance);
    if (metric != nullptr) {
      for (int i = 0; i < numOuts; i++) {
        fprintf(confFp, "  begin out%d\n", i);
        fprintf(confFp, "    int D %d\n", metric[2]);
        fprintf(confFp, "    int E %d\n", (metric[1] / numOuts));
        fprintf(confFp, "  end\n");
      }
    }
    if (metric != nullptr) {
      fprintf(confFp, "  real leakage %de-9\n", metric[0]);
      fprintf(confFp, "  int area %d\n", metric[3]);
    }
    fprintf(confFp, "end\n");
  }
}

void ChpProcGenerator::createMerge(FILE *libFp, FILE *confFp, const char *procName, const char *instance, int *metric, int numInputs) {
  if (!hasProcess(procName)) {
    fprintf(libFp, "template<pint W1, W2>\n");
    fprintf(libFp, "defproc %s(chan?(int<W1>)ctrl; ", procName);
    int i = 0;
    for (i = 0; i < numInputs; i++) {
      fprintf(libFp, "chan?(int<W2>) in%d; ", i);
    }
    fprintf(libFp, "chan!(int<W2>) out) {\n");
    fprintf(libFp, "  int<W1> c;\n  int<W2> x;\n");
    fprintf(libFp, "  chp {\n");
    fprintf(libFp, "    *[ctrl?c; log(\"receive \", c); [");
    for (i = 0; i < numInputs - 1; i++) {
      fprintf(libFp, "c=%d -> in%d?x [] ", i, i);
    }
    fprintf(libFp, "c=%d -> in%d?x]; log(\"receive x: \", x); ", i, i);
    fprintf(libFp, "out!x; log(\"send \", x)]\n");
    fprintf(libFp, "  }\n}\n\n");
  }
  if (!hasInstance(instance)) {
    fprintf(confFp, "begin %s\n", instance);
    fprintf(confFp, "  begin out\n");
    if (metric != nullptr) {
      fprintf(confFp, "    int D %d\n", metric[2]);
      fprintf(confFp, "    int E %d\n", metric[1]);
    }
    fprintf(confFp, "  end\n");
    if (metric != nullptr) {
      fprintf(confFp, "  real leakage %de-9\n", metric[0]);
      fprintf(confFp, "  int area %d\n", metric[3]);
    }
    fprintf(confFp, "end\n");
  }
}

void ChpProcGenerator::createSplit(FILE *libFp, FILE *confFp, const char *procName, const char *instance, int *metric,
                                   int numOutputs) {
  if (!hasProcess(procName)) {
    fprintf(libFp, "template<pint W1,W2>\n");
    fprintf(libFp, "defproc %s(chan?(int<W1>)ctrl; chan?(int<W2>)in; ", procName);
    int i = 0;
    for (i = 0; i < numOutputs - 1; i++) {
      fprintf(libFp, "chan!(int<W2>) out%d; ", i);
    }
    fprintf(libFp, "chan!(int<W2>) out%d) {\n", i);
    fprintf(libFp, "  int<W1> c;\n  int<W2> x;\n");
    fprintf(libFp, "  chp {\n");
    fprintf(libFp, "    *[in?x, ctrl?c; log(\"receive \", c, \", \", x);[");

    for (i = 0; i < numOutputs - 1; i++) {
      fprintf(libFp, "c=%d -> out%d!x [] ", i, i);
    }
    fprintf(libFp, "c=%d -> out%d!x]; log(\"send \", x)]\n", i, i);
    fprintf(libFp, "  }\n}\n\n");
  }
  if (!hasInstance(instance)) {
    fprintf(confFp, "begin %s\n", instance);
    if (metric != nullptr) {
      for (int i = 0; i < numOutputs; i++) {
        fprintf(confFp, "  begin out%d\n", i);
        fprintf(confFp, "    int D %d\n", metric[2]);
        fprintf(confFp, "    int E %d\n", metric[1]);
        fprintf(confFp, "  end\n");
      }
      fprintf(confFp, "  real leakage %de-9\n", metric[0]);
      fprintf(confFp, "  int area %d\n", metric[3]);
    }
    fprintf(confFp, "end\n");
  }
}

void ChpProcGenerator::createSource(FILE *libFp, FILE *confFp, const char *instance,
                                    int *metric) {
  if (!hasProcess("source")) {
    fprintf(libFp, "template<pint V, W>\n");
    fprintf(libFp, "defproc source(chan!(int<W>)x) {\n");
    fprintf(libFp, "  chp {\n");
    fprintf(libFp, "    *[log(\"send \", V); x!V]\n");
    fprintf(libFp, "  }\n}\n\n");
  }
  if (!hasInstance(instance)) {
    fprintf(confFp, "begin %s\n", instance);
    fprintf(confFp, "  begin x\n");
    if (metric != nullptr) {
      fprintf(confFp, "    int D %d\n", metric[2]);
      fprintf(confFp, "    int E %d\n", metric[1]);
    }
    fprintf(confFp, "  end\n");
    if (metric != nullptr) {
      fprintf(confFp, "  real leakage %de-9\n", metric[0]);
      fprintf(confFp, "  int area %d\n", metric[3]);
    }
    fprintf(confFp, "end\n");
  }
}

void ChpProcGenerator::createInit(FILE *libFp, FILE *confFp, const char *instance, int *metric) {
  if (!hasProcess("init")) {
    fprintf(libFp, "template<pint V, W>\n");
    fprintf(libFp,
            "defproc init(chan?(int<W>)in; chan!(int<W>) out) {\n");
    fprintf(libFp, "  int<W> x;\n");
    fprintf(libFp, "  bool b;\n");
    fprintf(libFp, "  chp {\n    b-;\n");
    fprintf(libFp,
            "    *[[~b->x:=V;b+ [] b->in?x]; out!x; log(\"send \", x)]\n  }\n}\n\n");
  }
  if (!hasInstance(instance)) {
    fprintf(confFp, "begin %s\n", instance);
    fprintf(confFp, "  begin in\n");
    fprintf(confFp, "    int D 0\n");
    fprintf(confFp, "    int E 0\n");
    fprintf(confFp, "  end\n");
    fprintf(confFp, "  begin out\n");
    if (metric != nullptr) {
      fprintf(confFp, "    int D %d\n", metric[2]);
      fprintf(confFp, "    int E %d\n", metric[1]);
    }
    fprintf(confFp, "  end\n");
    if (metric != nullptr) {
      fprintf(confFp, "  real leakage %de-9\n", metric[0]);
      fprintf(confFp, "  int area %d\n", metric[3]);
    }
    fprintf(confFp, "end\n");
  }
}

void ChpProcGenerator::createBuff(FILE *libFp, FILE *confFp, const char *instance, int *metric) {
  if (!hasProcess("buffer")) {
    fprintf(libFp, "template<pint W>\n");
    fprintf(libFp,
            "defproc buffer(chan?(int<W>)in; chan!(int<W>) out) {\n");
    fprintf(libFp, "  int<W> x;\n");
    fprintf(libFp, "  chp {\n    *[in?x; out!x; log(\"send \", x)]\n  }\n}\n\n");
  }
  if (!hasInstance(instance)) {
    fprintf(confFp, "begin %s\n", instance);
    fprintf(confFp, "  begin in\n");
    fprintf(confFp, "    int D 0\n");
    fprintf(confFp, "    int E 0\n");
    fprintf(confFp, "  end\n");
    fprintf(confFp, "  begin out\n");
    if (metric != nullptr) {
      fprintf(confFp, "    int D %d\n", metric[2]);
      fprintf(confFp, "    int E %d\n", metric[1]);
    }
    fprintf(confFp, "  end\n");
    if (metric != nullptr) {
      fprintf(confFp, "  real leakage %de-9\n", metric[0]);
      fprintf(confFp, "  int area %d\n", metric[3]);
    }
    fprintf(confFp, "end\n");
  }
}

void ChpProcGenerator::createSink(FILE *libFp, FILE *confFp, const char *instance, int *metric) {
  if (!hasProcess("sink")) {
    fprintf(libFp, "template<pint W>\n");
    fprintf(libFp, "defproc sink(chan?(int<W>) in) {\n");
    fprintf(libFp, "  int<W> t;");
    fprintf(libFp, "  chp {\n");
    fprintf(libFp, "  *[in?t; log (\"got \", t)]\n");
    fprintf(libFp, "  }\n}\n");
  }
  if (!hasInstance(instance)) {
    fprintf(confFp, "begin %s\n", instance);
    fprintf(confFp, "  begin x\n");
    if (metric != nullptr) {
      fprintf(confFp, "    int D %d\n", metric[2]);
      fprintf(confFp, "    int E %d\n", metric[1]);
    }
    fprintf(confFp, "  end\n");
    if (metric != nullptr) {
      fprintf(confFp, "  real leakage %de-9\n", metric[0]);
      fprintf(confFp, "  int area %d\n", metric[3]);
    }
    fprintf(confFp, "end\n");
  }
}

void ChpProcGenerator::createCopy(FILE *libFp, FILE *confFp, const char *instance, int *metric) {
  if (!hasProcess("copy")) {
    fprintf(libFp, "template<pint W, N>\n");
    fprintf(libFp, "defproc copy(chan?(int<W>) in; chan!(int<W>) out[N]) {\n");
    fprintf(libFp, "  int<W> x;\n  chp {\n");

    fprintf(libFp, "  *[ in?x; log(\"receive \", x); "
                   "(,i:N: out[i]!x; log(\"send \", i, \",\", x) )]\n");

//    fprintf(libFp, "  *[ in?x; (,i:N: out[i]!x); log(\"send \", x) ]\n");
    fprintf(libFp, "  }\n}\n\n");
  }
  if (!hasInstance(instance)) {
    fprintf(confFp, "begin %s\n", instance);
    fprintf(confFp, "  begin in\n");
    fprintf(confFp, "    int D 0\n");
    fprintf(confFp, "    int E 0\n");
    fprintf(confFp, "  end\n");
    fprintf(confFp, "  begin out\n");
    if (metric != nullptr) {
      fprintf(confFp, "    int D %d\n", metric[2]);
      fprintf(confFp, "    int E %d\n", metric[1]);
    }
    fprintf(confFp, "  end\n");
    if (metric != nullptr) {
      fprintf(confFp, "  real leakage %de-9\n", metric[0]);
      fprintf(confFp, "  int area %d\n", metric[3]);
    }
    fprintf(confFp, "end\n");
  }
}
