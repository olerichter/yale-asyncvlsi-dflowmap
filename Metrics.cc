#include "Metrics.h"

void Metrics::updateMetrics(const char *op, int *metric) {
  if (DEBUG_VERBOSE) {
    printf("Update metrics for %s\n", op);
  }
  for (auto &opMetricsIt : opMetrics) {
    if (!strcmp(opMetricsIt.first, op)) {
      fatal_error("We already have metric info for %s", op);
    }
  }
  opMetrics.insert(std::make_pair(op, metric));
}

void Metrics::normalizeName(char *src, char toDel, char newChar) {
  char *pos = strchr(src, toDel);
  while (pos) {
    *pos = newChar;
    pos = strchr(pos + 1, toDel);
  }
}

int *Metrics::getOpMetric(const char *op) {
  if (DEBUG_VERBOSE) {
    printf("get op metric for %s\n", op);
  }
  if (op == nullptr) {
    fatal_error("op is NULL\n");
  }
  char *normalizedOp = new char[10240];
  normalizedOp[0] = '\0';
  strcat(normalizedOp, op);
  normalizeName(normalizedOp, '<', '_');
  normalizeName(normalizedOp, '>', '_');
  normalizeName(normalizedOp, ',', '_');
  for (auto &opMetricsIt : opMetrics) {
    if (!strcmp(opMetricsIt.first, normalizedOp)) {
      return opMetricsIt.second;
    }
  }
  printf("\n\n\n%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n"
         "We could not find metric info for %s\n", normalizedOp);
  if (DEBUG_VERBOSE) {
    printOpMetrics();
    printf("\n\n\n\n\n");
  }
  return nullptr;
}

void Metrics::printOpMetrics() {
  printf("Info in opMetrics:\n");
  for (auto &opMetricsIt : opMetrics) {
    printf("%s ", opMetricsIt.first);
  }
  printf("\n");
}

void Metrics::readMetricsFile(const char *metricFilePath) {
  printf("Read metric file: %s\n", metricFilePath);
  std::ifstream metricFp(metricFilePath);
  std::string line;
  while (std::getline(metricFp, line)) {
    std::istringstream iss(line);
    char *instance = new char[10240];
    int metricCount = -1;
    int *metric = new int[4];
    bool emptyLine = true;
    do {
      std::string numStr;
      iss >> numStr;
      if (!numStr.empty()) {
        emptyLine = false;
        if (metricCount >= 0) {
          metric[metricCount] = std::atoi(numStr.c_str());
        } else {
          sprintf(instance, "%s", numStr.c_str());
        }
        metricCount++;
      }
    } while (iss);
    if (!emptyLine && (metricCount != 4)) {
      printf("%s has %d metrics!\n", metricFilePath, metricCount);
      exit(-1);
    }
    if (DEBUG_VERBOSE) {
      printf("Add metrics for %s\n", instance);
    }
    updateMetrics(instance, metric);
  }
}

Metrics::Metrics() {}

void Metrics::updateCopyStatistics(int bitwidth, int numOutputs) {
  auto copyStatisticsIt = copyStatistics.find(bitwidth);
  if (copyStatisticsIt != copyStatistics.end()) {
    Map<int, int> &record = copyStatisticsIt->second;
    auto recordIt = record.find(numOutputs);
    if (recordIt != record.end()) {
      recordIt->second++;
    } else {
      record.insert(GenPair(numOutputs, 1));
    }
  } else {
    Map<int, int> record = {{numOutputs, 1}};
    copyStatistics.insert(GenPair(bitwidth, record));
  }
}

void Metrics::printCopyStatistics() {
  printf("COPY STATISTICS:\n");
  for (auto &copyStatisticsIt : copyStatistics) {
    printf("%d-bit COPY:\n", copyStatisticsIt.first);
    Map<int, int> &record = copyStatisticsIt.second;
    for (auto &recordIt : record) {
      printf("  %d outputs: %d\n", recordIt.first, recordIt.second);
    }
  }
  printf("\n");
}

void Metrics::updateAreaStatistics(const char *instance, int area) {
  totalArea += area;
  for (auto &areaStatisticsIt : areaStatistics) {
    if (!strcmp(areaStatisticsIt.first, instance)) {
      areaStatisticsIt.second += area;
      return;
    }
  }
  areaStatistics.insert(GenPair(instance, area));
}

void Metrics::printAreaStatistics() {
  printf("Area Statistics:\n");
  std::multimap<int, const char *> sortedAreas = flip_map(areaStatistics);
  std::multimap<int, const char *>::iterator sortedAreasIt;
  for (auto iter = sortedAreas.rbegin(); iter != sortedAreas.rend(); ++iter) {
    int area = iter->first;
    double ratio = (double) area / totalArea * 100;
//    if (ratio > 1) {
    printf("%40.30s %5d %5.1f\%\n", iter->second, area, ratio);
//    }
  }
  printf("\n");
}

void Metrics::dump() {
  printOpMetrics();
  printCopyStatistics();
  printAreaStatistics();
}
