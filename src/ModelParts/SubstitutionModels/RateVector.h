#ifndef RateVector_h_
#define RateVector_h_

#include <vector>
#include <functional>
#include <string>
#include <list>
#include <map>

#include "States.h"
#include "../AbstractComponent.h"
#include "../../IO/SubstitutionModelParser.h"

struct rv_request {
  // Struct representing a request for a rate vector.
  // More specific requests later, for example branch position.
  unsigned int pos;
  int state;
  std::string domain;
  std::map<std::string, int> ex_state; // Extended state.
};

class BranchSegment; // Defined in Trees/Types/TreeParts.h

// Fundamental collection type of parameters in substitution model.
class RateVector {
private:
  int id;
  static int IDc;
  std::string domain; // Name of the state set.
  std::string name;
  const States* states;
public:
  RateVector(std::string, std::string, const States*, std::vector<Valuable*>);

  std::vector<Valuable*> rates;
  int state; // Determines the (ancestral) state that this rate vector applies to.
  float operator[](int);
  float get_rate_ratio(int i);
  const int& getID();

  int size();
  const std::string& get_name();
  std::string get_state();
  std::string get_state_by_pos(int);

  void print();
};

struct rv_loc {
  // Rate Vector Locations
  RateVector* rv;
  int pos;
};

typedef std::map<std::string, std::string> ExtendedState; // Domain -> State;
ExtendedState ExtendedState_Null();
std::string ExtendedState_toString(ExtendedState);

// Collections of rate vectors.
class RateVectorSet {
  // This collection holds all of the rate vectors currently in the substitution model.
public:
  RateVectorSet();
  std::vector<RateVector*> col;
  void Initialize(States states, std::map<std::string, States>);

  std::list<std::list<signed char>> configure_hash(std::map<std::string, States> all_states);

  void add(RateVector* v, IO::rv_use_class);
  void organize();

  RateVector* select(rv_request);
  std::map<std::string, int> get_ExtendedState(const std::map<std::string, std::vector<signed char>*>& sequences, int pos);
  const std::list<rv_loc>& get_host_vectors(Valuable*);

  void print();
  void saveToFile(int gen, double l);
 private:
  std::map<std::string, States> all_states;
  unsigned int n_domains;
  std::list<std::string> domain_names;

  std::map<std::string, std::map<ExtendedState, RateVector*>> ex_state_to_rv; // Domain -> ExState -> RateVector*;
  std::map<std::string, std::map<unsigned long, RateVector*>> state_to_rv; // Domain -> ExState -> RateVector*;

  // Hash stuff
  std::list<signed char> ex_to_list(ExtendedState);
  
  // Old
  std::map<int, IO::rv_use_class> id_to_uc; // Only used to before RateVectors are organized.

  // Tree structure of Rate Vectors.
  // Organized via positions -> states -> possible RateVectors.
  std::map<Valuable*, std::list<rv_loc>> parameter_locations; // Param ID -> rate vector locations.
};

#endif
