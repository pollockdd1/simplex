#ifndef SequencesParser_h_
#define SequencesParser_h_

#include <map>
#include <list>
#include <algorithm>
#include "Files.h"

namespace IO {
  struct RawMSA {
    std::map<std::string, std::string> seqs;
    unsigned int n = 0;
    unsigned int cols = 0;
  };

  // Operators
  bool operator==(const RawMSA& lhs, const RawMSA& rhs);
  std::ostream& operator<<(std::ostream& os, const RawMSA& msa);

  // Utils
  RawMSA* readRawMSA(std::string file_name);
  void printRawMSA(const RawMSA& msa);
  std::list<std::string> getRawMSANames(const RawMSA& msa);
  void convertToGaps(RawMSA& msa, std::list<std::string> remove_list);
}

#endif
