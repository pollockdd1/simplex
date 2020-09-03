#ifndef RawParameterTypes_h_
#define RawParameterTypes_h_

#include <iostream>
#include <list>

#include "../ModelParts/SubstitutionModels/Parameters.h"
#include "sol2/sol.hpp"

namespace IO { 
  class ParameterWrapper {
  private:
    std::string name;
  public:
    ParameterWrapper(AbstractComponent*);
    std::string get_name();
    std::string get_type();
    void set_upper_bound(ParameterWrapper*);
    void set_lower_bound(ParameterWrapper*);
    AbstractComponent* parameter;
  };

  ParameterWrapper new_parameter(std::string, std::string, sol::table);
  ParameterWrapper new_categories(std::string, sol::table);

  ParameterWrapper add_parameters(ParameterWrapper, ParameterWrapper);
  ParameterWrapper named_add_parameters(std::string, ParameterWrapper, ParameterWrapper);
  ParameterWrapper subtract_parameters(ParameterWrapper, ParameterWrapper);
  ParameterWrapper named_subtract_parameters(std::string, ParameterWrapper, ParameterWrapper);
  ParameterWrapper multiply_parameters(ParameterWrapper, ParameterWrapper);
  ParameterWrapper named_multiply_parameters(std::string, ParameterWrapper, ParameterWrapper);
  ParameterWrapper divide_parameters(ParameterWrapper, ParameterWrapper);
  ParameterWrapper named_divide_parameters(std::string, ParameterWrapper, ParameterWrapper);
}

#endif
