#include "MCMC.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <limits>
#include <chrono>

#include "Model.h"
#include "IO/Files.h"


std::ofstream MCMC::lnlout;

extern double Random();
extern Environment env;
extern IO::Files files;

ofstream MCMC::time_out;

/// Public Functions ///

MCMC::MCMC() {
	/*
	 * Default constructor.
	 */
	model = 0;
	gen = 0;
	gens = 0;
	lnL = 0;
} 

void MCMC::initialize(Model* model) {
  /*
   * Init MCMC with model, gens calculate lnL.
   */

  std::cout << "\nInitializing MCMC." << std::endl;
  this->model = model; // associate the pointer with the MCMC

  // Env settings.
  out_freq = env.get<int>("MCMC.output_frequency");
  print_freq = env.get<int>("MCMC.print_frequency");
  gens = env.get<int>("MCMC.generations");
  tree_sample_freq = env.get<int>("MCMC.tree_sample_frequency");

  //Calculate initial likelihood.
  lnL = model->CalculateLikelihood();

  RecordState();

  //Initialize output file.
  files.add_file("likelihoods", env.get<std::string>("OUTPUT.likelihood_out_file"), IOtype::OUTPUT);
  lnlout = files.get_ofstream("likelihoods");
  lnlout << "I,GEN,LogL" << std::endl;

  model->printParameters();
}

void MCMC::sample() {
  static int i = 1;
  bool sampleType;

  if(i % tree_sample_freq == 0) {
    sampleType = model->SampleTree(); // All tree sampling right now is Gibbs.
    lnL = model->CalculateLikelihood();
    i = 0;
  } else {

    sampleType = model->SampleSubstitutionModel();
    newLnL = model->updateLikelihood();
    if(sampleType) {
      //Metropolis-Hasting method.
      if (log(Random()) <= (newLnL - lnL)) {
	lnL = newLnL;
	model->accept();
      } else {
	model->reject();
      }
    } else {
      // No Metropolis Hastings needed - Gibbs sampling.
      lnL = newLnL;
      model->accept();
    }
  }
  i++;
}

void MCMC::Run() {
  /*
   * Run an initialized MCMC.
   */

  std::cout << "Starting MCMC:" << std::endl;
  for (gen = 1; gen <= gens; gen++) {
    sample();

    if(isnan(lnL)) {
      std::cerr << "Error: LogLikelihood is Nan." << std::endl;
      exit(EXIT_FAILURE);
    }

    if(gen % print_freq == 0) {
      std::cout << "Likelihood: " << lnL << std::endl;
      // model->printParameters();
    }

    if(gen % out_freq == 0) {
      RecordState();
    }
  }
}

///  Private Functions  ///
void MCMC::RecordState() {  // ought to use a function to return tab separated items with endl
  static int i = -1;
  i++;
  lnlout << i << "," << gen << "," << lnL << std::endl;
  model->RecordState(gen, lnL); // is this always getting lnlout?
}
