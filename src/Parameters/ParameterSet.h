#ifndef ParameterSet_h_
#define ParameterSet_h_

#include <list>
#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <fstream>

#include "AbstractValue.h"
#include "RateVector.h"

class ParameterSet {
	public:
		ParameterSet();
		void Initialize(std::ofstream* &out_file_buffer);
		void add_parameter(AbstractParameter* param);
		void add_rate_vector(RateVector* v);
		void add_rate_matrix(RateMatrix* Q);

		void sample();
		void accept();
		void reject();

		void print();
		double get(const std::string &name);
		void RecordStateToFile();
	private:
		void stepToNextParameter();
		void AddHeaderToFile();

		std::list<AbstractHyperParameter*> hyperparameter_list;
		std::list<AbstractParameter*> parameter_list;
		std::list<AbstractParameter*>::iterator current_parameter; //Tracks the current parameter to be sampled, via an iterator across the parameter_list.
	
		// Dependancies.
		std::map<AbstractValue*, std::list<AbstractHyperParameter*>> value_to_dependents; // Maps AbstractValues to AbstractHyperParameters that depend on them.
		void setupDependancies();
		void refreshDependancies(AbstractValue*);

		std::map<std::string, AbstractParameter*> name_to_address; //A map from the name of a parameter to the pointer of the parameter class.
		std::ofstream* out_stream_buffer;
};

#endif
