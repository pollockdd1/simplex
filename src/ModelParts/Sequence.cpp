#include <sstream>
#include <algorithm>
#include <cassert>

#include "Sequence.h"
#include "../Environment.h"
#include "../IO/Files.h"

#include "Trees/TreeParts.h"
#include "Trees/Tree.h"

// Globals
extern double Random();
extern Environment env;
extern IO::Files files;

std::vector<std::string> aa({"A", "R", "N", "D", "C", "E", "Q", "G", "H", "I", "L", "K", "M", "F", "P", "S", "T", "W", "Y", "V"});
std::vector<std::string> nucleotides({"A", "T", "C", "G"});

static const state_element gap_indicator = -1;

// Sequence Alignment class.

SequenceAlignment::SequenceAlignment(std::string name, std::string msa_out, std::string subs_out, const States* states) : domain_name(name) {
  this->states = states->possible;
  this->n_states = states->n;

  this->state_element_encode = states->state_to_int;
  this->state_element_decode = states->int_to_state;

  this->seqs_out_file = msa_out;
  this->substitutions_out_file = subs_out;
}

void SequenceAlignment::add_internal(std::string name) {
  // Adds sequence to alignment, that WILL be sampled during MCMC.
  // This is for the ancestral nodes.
  assert(n_columns > 0);
  taxa_names_to_sequences[name] = std::vector<state_element>(n_columns, -1);
  taxa_names_to_gaps[name] = std::vector<bool>(n_columns, true);
}

double** create_state_probability_vector(unsigned int n_cols, unsigned int n_states) {
  double** m = new double*[n_cols];
  for(unsigned int i = 0; i < n_cols; i++) {
    m[i] = new double[n_states];
    for(unsigned int j = 0; j < n_states; j++) {
      m[i][j] = 0.0;
    }
  }

  return(m);
}

void SequenceAlignment::add_base(std::string name, const IO::FreqSequence &seq) {
  taxa_names_to_sequences[name] = encode_sequence(sequenceAsStr_highestFreq(seq));

  prior_state_distribution[name] = create_state_probability_vector(seq.size(), n_states);

  int pos = 0;
  // Loop through each position in Frequency Sequence
  for(auto it = seq.begin(); it != seq.end(); ++it) {
    for(auto jt = it->begin(); jt != it->end(); ++jt) {
      // Loop through states.
      if(jt->state != '-') {
        prior_state_distribution[name][pos][state_element_encode[std::string(1, jt->state)]] = jt->freq;
      } else {
        assert(jt->freq == 1.0);
      }
    }
    pos++;
  }

  // Set gaps
  std::vector<state_element> sequence = taxa_names_to_sequences[name];
  std::vector<bool> gaps(sequence.size(), false);
  for(unsigned int pos = 0; pos < sequence.size(); pos++) {
    if(sequence.at(pos) == -1) {
      gaps[pos] = true;
    } else {
      gaps[pos] = false;
    }
  }

  taxa_names_to_gaps[name] = gaps;
}

void SequenceAlignment::print() {
  std::cout << "SEQUENCES" << std::endl;
  //for(std::map<std::string, std::vector<state_element>>::iterator it = taxa_names_to_sequences.begin(); it != taxa_names_to_sequences.end(); ++it) {
  for(const auto& [taxa_name, sequence] : this->taxa_names_to_sequences) {
    std::cout << ">" << taxa_name << "\n" << decode_state_element_sequence(sequence) << std::endl;
  }
}

void SequenceAlignment::initialize_common(IO::RawMSA raw_msa) {   
  for(auto it = raw_msa.seqs.begin(); it != raw_msa.seqs.end(); ++it) {
    add_base(it->first, it->second);
  }

  // Setup output.
  seqs_out_identifier = domain_name + "_sequences_out";
  files.add_file(seqs_out_identifier, seqs_out_file, IOtype::OUTPUT);

  substitutions_out_identifier = domain_name + "_substitutions_out";
  files.add_file(substitutions_out_identifier, substitutions_out_file, IOtype::OUTPUT);
  files.write_to_file(substitutions_out_identifier, "I,GEN,LogL,Ancestral,Decendant,Substitutions\n");

  // ASSUMES columns are same length.
  this->n_columns = (*taxa_names_to_sequences.begin()).second.size();
}

void SequenceAlignment::initialize_dynamic(IO::RawMSA raw_msa) {
  this->tag = Tag::DYNAMIC;

  this->initialize_common(raw_msa);
}

void SequenceAlignment::initialize_site_static(IO::RawMSA raw_msa) {
  this->tag = Tag::SITE_STATIC;
 
  this->initialize_common(raw_msa);

  // VALIDATE that states are consistant across columns.
  // And that nonoe of the sites have uncertain priors e.g. [A:0.5,B:0.5] etc.
  std::vector<state_element> column_states(this->n_columns, -1);
  for (const auto& [key, sequence] : this->taxa_names_to_sequences) {
    for (size_t pos = 0; pos < sequence.size(); ++pos) {
      if (column_states[pos] == -1) column_states[pos] = sequence[pos];
      if (sequence[pos] != -1 and sequence[pos] != column_states[pos]) {
        exit(EXIT_FAILURE);
      }

      for(size_t i = 0; i < this->states.size(); ++i) {
        double state_probability = this->prior_state_distribution[key][pos][i];
        if (state_probability != 0.0 and state_probability != 1.0) {
          std::cout << "Error: uncertain state in SITE_STATIC state domain \'" << this->domain_name << "\'." << std::endl;
          exit(EXIT_FAILURE);
        }
      }
    }
  }
}

void SequenceAlignment::saveToFile(int save_count, uint128_t gen, double l) {
  std::ostringstream buffer;
  buffer << "#" << save_count << ":" << gen << ":" << l << std::endl;
  for(auto it = taxa_names_to_sequences.begin(); it != taxa_names_to_sequences.end(); ++it) {
    buffer << ">" << it->first << "\n" << decode_state_element_sequence(it->second) << std::endl;
  }

  files.write_to_file(seqs_out_identifier, buffer.str());

  std::ostringstream subs_buffer;

  // Save the substitutions for a particular state.
  std::list<BranchSegment*> branches = tree->get_branches();
  for(auto it = branches.begin(); it != branches.end(); ++it) {
    subs_buffer << save_count << "," << gen << "," << l << ",";
    subs_buffer << (*it)->ancestral->name << "," << (*it)->decendant->name << ",[ ";
    std::vector<Substitution> subs = (*it)->get_substitutions(domain_name);
    for(unsigned int pos = 0; pos < subs.size(); pos++) {
      if(subs[pos].occuredp == true) {
        int anc = (*it)->ancestral->sequences[domain_name]->at(pos);
        int dec = (*it)->decendant->sequences[domain_name]->at(pos);

        // Includes virtual substitutions.
        subs_buffer << state_element_decode[anc] << pos << state_element_decode[dec] << " ";
      }
    }
    subs_buffer << "]\n";
  }
  files.write_to_file(substitutions_out_identifier, subs_buffer.str());
}

void SequenceAlignment::syncWithTree(std::string domain_name, Tree* tree) {
  /*
    Connects all the tree nodes on the matching sequences in the MSAs for each state domain.
    Add new sequences to MSA for nodes present on the tree but missing in the alignment.
    This results in pointers from the TreeNodes to the sequence vectors.
   */

  this->tree = tree;
  std::cout << "\tAttaching \'" << domain_name << "\' states to tree." << std::endl;

  for(const auto& node : tree->nodes()) {
    this->marginal_state_distribution[node->name] = create_state_probability_vector(this->n_columns, this->n_states);

    if(taxa_names_to_sequences.count(node->name)) {
      node->sequences[domain_name] = &(this->taxa_names_to_sequences.at(node->name));
    } else {
      if(node->isTip()){
        std::cerr << "Error: Missing sequence for \"" << node->name << "\"." << std::endl;
        exit(EXIT_FAILURE);
      } else {
        // Add new sequence to sequence alignments.
        this->add_internal(node->name);
        node->sequences[domain_name] = &(this->taxa_names_to_sequences.at(node->name));	
      }
    }
  }

  // Set gaps for internal nodes.
  for(const auto& node : tree->nodes()) {
    if(node->isTip()) continue;

    if(node->left != 0 and node->right == 0) {
      // Internal Continous.
      TreeNode* dsNode = node->left->decendant; // ds = downstream.
      for(unsigned int i = 0; i < dsNode->sequences[domain_name]->size(); i++) {
        this->taxa_names_to_gaps[node->name][i] = this->taxa_names_to_gaps[dsNode->name][i];
      }
    } else {
      for(unsigned int pos = 0; pos < node->sequences[domain_name]->size(); pos++) {
        // Checks left branch first - there is always a left branch, except tips.
        if(this->taxa_names_to_gaps[node->left->decendant->name][pos]) {
          // Checks right branch next - right branches only on branching segment.
          if(node->right) {
            this->taxa_names_to_gaps[node->name][pos] = this->taxa_names_to_gaps[node->right->decendant->name][pos];
          } else {
            this->taxa_names_to_gaps[node->name][pos] = true;
          }
        } else {
          this->taxa_names_to_gaps[node->name][pos] = false;
        }
      }
    }
  }

  // Set initial states of internal sequences.
  for(unsigned int i = 0; i < n_columns ; i++) find_parsimony_by_position(i);
}

// Reading Fasta files.
std::vector<state_element> SequenceAlignment::encode_sequence(const std::string &sequence) {
  /*
   * Takes a string representation of a sequence and returns vector of integers.
   * Also tracks the gaps in the alignment.
   */
  std::vector<state_element> encoded_sequence(sequence.length());

  for (unsigned int pos = 0; pos < sequence.length(); pos++) {
    std::string current_state = sequence.substr(pos, 1);
    try {
      encoded_sequence.at(pos) = this->state_element_encode.at(current_state);
    } catch(const std::out_of_range& e) {
      // Temporary solution.
      if (current_state == "-") {
        encoded_sequence.at(pos) = -1;
      } else {
        std::cerr << "Error: state \"" << current_state << "\" in sequence alignment is not recognised. " << std::endl;
        exit(EXIT_FAILURE);
      }
    }
  }

  return(encoded_sequence);
}

// Utilities
unsigned int SequenceAlignment::n_cols() {
  return(n_columns);
}

std::string SequenceAlignment::decode_state_element(state_element c) {
  return(state_element_decode[c]);
}

std::string SequenceAlignment::decode_state_element_sequence(const std::vector<state_element> &enc_seq) {
  std::string decoded_sequence;
  for(const auto& element : enc_seq) decoded_sequence.append(decode_state_element(element));
  return(decoded_sequence);
}

// PARSIMONY
state_element pick_most_frequent_state(const std::vector<state_element> &clade_states, state_element above) {
  std::map<state_element, int> state_counts = {};
  for (const auto& it : clade_states) {
    if(state_counts.find(it) == state_counts.end()) {
      state_counts[it] = 1; // If state is not observed yet, initiate counts.
    } else {
      state_counts[it] += 1;
    }
  }

  // Picks the most frequent states from the states in the clade below. If the counts are equal the state
  // is used to influence the choice.
  state_element most_frequent = 0;
  int highest_count = 0;
  for (const auto& [state, count]: state_counts) {
    if((count > highest_count) or ((count == highest_count) and (state == above) and (above != -1))) {
      most_frequent = state;
      highest_count = count;
    }
  }
  return(most_frequent);
}

void SequenceAlignment::find_parsimony_by_position(unsigned int pos) {
  /*
   * This is not really parsimony at all, just selects the most common state observed
   * at the tips in the clade below.
   */
  // Node names -> list of all states observed below a given node at a given site.
  std::map<std::string, std::vector<state_element>> clade_states = {};

  for (TreeNode* n : tree->nodes()) { // Recursively traverses the tree from the bottom up.
    if(taxa_names_to_gaps[n->name][pos]) continue;

    if(n->isTip()) {
      clade_states[n->name] = {n->sequences[domain_name]->at(pos)};
    } else {
      clade_states[n->name] = {};
    }

    if(n->left != 0) {
      for (const state_element& state : clade_states[n->left->decendant->name]) {
        clade_states[n->name].push_back(state);
      }
    }
    if(n->right != 0) {
      for (const state_element& state : clade_states[n->right->decendant->name]) {
        clade_states[n->name].push_back(state);
      }
    }
  }

  for(auto it = tree->nodes().rbegin(); it != tree->nodes().rend(); ++it) {
    TreeNode* n = *it;
    if(n->isTip() or taxa_names_to_gaps[n->name][pos]) continue;
    
    int state_above;
    if(n->up == nullptr) {
      state_above = -1; // ROOT
    } else {
      state_above = (*n->up->ancestral->sequences[domain_name])[pos];
    }

    //std::cout << n->name << " [ ";
    //for(auto jt = clade_states[n->name].begin(); jt != clade_states[n->name].end(); ++jt) {
    //  std::cout << (unsigned int)*jt << " ";
    //}
    //std::cout << "] " << (unsigned int)pick_most_frequent_state(clade_states[n->name], state_above) << std::endl;
    
    (*n->sequences[domain_name])[pos] = pick_most_frequent_state(clade_states[n->name], state_above);
  }
}

// SAMPLING
void SequenceAlignment::reset_to_base(std::string node_name, const std::list<unsigned int>& positions) {
  for (unsigned int pos : positions) {
    for(unsigned int i = 0; i < n_states; i++) {
      marginal_state_distribution[node_name][pos][i] = prior_state_distribution[node_name][pos][i];
    }
  } 
}

void SequenceAlignment::normalize_state_probs(const TreeNode* node, unsigned int pos) {
  double normalize_total = 0.0;

  for(unsigned int i = 0; i < n_states; i++) {
    normalize_total += this->marginal_state_distribution[node->name][pos][i];
  }

  if(normalize_total != 0.0) {
    for(unsigned int i = 0; i < n_states; i++) {
      this->marginal_state_distribution[node->name][pos][i] /= normalize_total;
    }
  }
}

// Calculating Probabilities.
inline double calc_substitution_prob(double rate, float t_b, double u) {
  /*
   * rate = substitution rate.
   * t_b = branch length.
   * u = uniformisation constant.
   */
  return((rate * t_b) / (1.0 + (u * t_b)));
}

inline double calc_no_substitution_prob(double rate, float t_b, double u) {
  // Probability that there is any virtual substitution | given no substitution - equation (9) Rapid Likelihood Analysis on Large Phylogenies.
  double prob_virtual = 1.0 - (1.0 / (1.0 + (rate * t_b)));
  double denom = 1.0 / (1.0 + (u * t_b));

  //     Virtual Substitution                       No substitution
  return((prob_virtual * ((rate * t_b) * denom)) + ((1.0 - prob_virtual) * denom));
}

double SequenceAlignment::find_state_prob_given_dec_branch(BranchSegment* branch,
                                                           state_element state_i,
                                                           double* state_probs,
                                                           std::vector<Valuable*> rv,
                                                           double u,
                                                           unsigned int pos) {
  /*
   * Find state probability given decendent branch
   * state_probs = the marginal posterior distribution of the state at the node below.
   */

  double prob = 0.0;
  double focal_domain_prob = 0.0;
  double alt_domain_prob = 1.0;

  float t_b = branch->distance;

  for(state_element state_j = 0; state_j < (state_element)n_states; state_j++) {
    double state_prob = state_probs[state_j];

    if(state_prob != 0.0) {
      // Likelihood contribution of all substitutions - including alternate domains.
      for(BranchSegment::iterator it = branch->begin(pos); it != branch->end(); it++) {
        std::string domain = (*it).first;

        if(domain == this->domain_name) {
          double rate = rv[state_j]->get_value();
          if(state_i != state_j) {
            // Normal Substitution
            focal_domain_prob = calc_substitution_prob(rate, t_b, u);
          } else {
            // No substitution - or possibly virtual.
            focal_domain_prob = calc_no_substitution_prob(rate, t_b, u);
          }
        } else if (not this->tree->get_SM()->is_static(domain)) {
          // Subsitutions in non focal domain.
          Substitution sub = (*it).second;
          std::map<std::string, state_element> context = {{domain, sub.anc_state},
                                                          {this->domain_name, state_i}};

          RateVector* rv = branch->get_hypothetical_rate_vector(domain, context, pos);

          if(sub.occuredp and (sub.anc_state != sub.dec_state)) {
            // Substitution including virtual substitutions.
            alt_domain_prob *= calc_substitution_prob(rv->rates[sub.dec_state]->get_value(), t_b, u);
          } else {
            alt_domain_prob *= calc_no_substitution_prob(rv->rates[sub.anc_state]->get_value(), t_b, u);
          }
        }	
      }
      prob += (state_prob * focal_domain_prob * alt_domain_prob);

      // Reset state-specific probability terms.
      focal_domain_prob = 0.0;
      alt_domain_prob = 1.0;
    }
  }

  return(prob);
}

double SequenceAlignment::find_state_prob_given_anc_branch(BranchSegment* branch, state_element state_j, double* state_probs, TreeNode* node, double u, unsigned int pos) {
  /*
   * Find state probability given ancestor branch.
   */

  double prob = 0.0;
  double focal_domain_prob = 0.0;
  double alt_domain_prob = 1.0;

  float t_b = branch->distance;

  for(state_element state_i = 0; state_i < (signed char)n_states; ++state_i) {
    double state_prob = state_probs[state_i];
    if(state_prob != 0.0) {
      for(BranchSegment::iterator it = branch->begin(pos); it != branch->end(); it++) {
        std::string domain = (*it).first;

        if(domain == this->domain_name) {
          // Focal Domain
          std::map<std::string, state_element> context = {{this->domain_name, state_i}};
          RateVector* rv = node->up->get_hypothetical_rate_vector(domain_name, context, pos);

          double rate = rv->rates[state_j]->get_value(); // i -> j rate.
          if(state_i != state_j) {
            // Normal Substitution.
            focal_domain_prob = calc_substitution_prob(rate, t_b, u); // Probability of the substitution.
          } else {
            // No substition - possibly virtual.
            focal_domain_prob = calc_no_substitution_prob(rate, t_b, u);
          }

        } else if (not this->tree->get_SM()->is_static(domain)) {
          // Alternative domains.
          Substitution sub = (*it).second;
          std::map<std::string, state_element> context = {{domain, sub.anc_state},
                                                          {this->domain_name, state_i}};
          
          RateVector* rv = branch->get_hypothetical_rate_vector(domain, context, pos);
          if(sub.occuredp and (sub.anc_state != sub.dec_state)) {
            //Substitution including virtual substitutions.
            alt_domain_prob *= calc_substitution_prob(rv->rates[sub.dec_state]->get_value(), t_b, u);
          } else {
            alt_domain_prob *= calc_no_substitution_prob(rv->rates[sub.anc_state]->get_value(), t_b, u);
          }
        }
      }
      prob += (state_prob * focal_domain_prob * alt_domain_prob);

      // Reset state-specific probability terms.
      focal_domain_prob = 0.0;
      alt_domain_prob = 1.0;
    }
  }

  return(prob);
}

void SequenceAlignment::find_marginal_at_pos(TreeNode* node, unsigned int pos, TreeNode* left_node, TreeNode* right_node, TreeNode* up_node) {
  double u = node->SM->get_u();
  double left_prob = 1.0;
  double right_prob = 1.0;
  double up_prob = 1.0;

  //unsigned long extended_state;
  RateVector* rv;

  for(state_element state_i = 0; state_i < (signed char)n_states; state_i++) {
    // Most likely to be 0.0 so evaluated first.
    if(up_node != nullptr) {
      if(not taxa_names_to_gaps[up_node->name][pos]) {
        up_prob = find_state_prob_given_anc_branch(node->up, state_i, marginal_state_distribution[up_node->name][pos], node, u, pos);

        // Return if probability if 0.0.
        if(up_prob == 0.0) {
          this->marginal_state_distribution[node->name][pos][state_i] = 0.0;
          return;
        }
      }
    }

    // Contribution of left branch.
    if((left_node != nullptr) and not taxa_names_to_gaps[left_node->name].at(pos)) {
      std::map<std::string, state_element> context = {{domain_name, state_i}};
      rv = left_node->up->get_hypothetical_rate_vector(domain_name, context, pos);

      left_prob = find_state_prob_given_dec_branch(left_node->up, state_i, marginal_state_distribution[left_node->name][pos], rv->rates, u, pos);

      if(left_prob == 0.0) {
        this->marginal_state_distribution[node->name][pos][state_i] = 0.0;
        return;
      }
    }

    // Contribution of right branch.
    if((right_node != nullptr) and (not taxa_names_to_gaps[right_node->name][pos])) {
      std::map<std::string, state_element> context = {{domain_name, state_i}};
      rv = right_node->up->get_hypothetical_rate_vector(domain_name, context, pos);

      right_prob = find_state_prob_given_dec_branch(right_node->up, state_i, marginal_state_distribution[right_node->name][pos], rv->rates, u, pos);
      if(right_prob == 0.0) {
        this->marginal_state_distribution[node->name][pos][state_i] = 0.0;
        return;
      }
    }

    this->marginal_state_distribution[node->name][pos][state_i] = left_prob * right_prob * up_prob;
  }
}

void SequenceAlignment::find_state_probs_dec_only(TreeNode* node, std::list<unsigned int> positions) {
  /*
   * Finds the marginal posterior distribution for each position at a given node.
   * Only uses infomation from nodes below - used for upward recursion.
   * Assumes that node is not a tip.
   */

  std::string name = node->name;
  std::vector<bool> gaps = taxa_names_to_gaps[name];
  if(not node->isTip()) {
    // Node may or may not be a branch node - therefore may only have one child which is always the left one.
    // Set to nullptr if no right branch;
    TreeNode* right_node;
    if(node->right) {
      right_node = node->right->decendant;
    } else {
      right_node = nullptr;
    }

    for (unsigned int pos : positions) {
      if(not gaps[pos]) {
        // Always a left node.
        find_marginal_at_pos(node, pos, node->left->decendant, right_node, nullptr);
        normalize_state_probs(node, pos);
      }
    }
  }
}

// TODO refactor.
void SequenceAlignment::find_state_probs_all(TreeNode* node, std::list<unsigned int> positions) {
  // NOTE assumes not a tip.
  std::string name = node->name;
  std::vector<bool> gaps = taxa_names_to_gaps[name];

  // Node may or may not be a branch node - therefore may only have one child which is always the left one.
  // Set to nullptr if no right branch;
  TreeNode* right_node;
  if(node->right) {
    right_node = node->right->decendant;
  } else {
    right_node = nullptr;
  }

  TreeNode* up_node;
  if(node->up) {
    up_node = node->up->ancestral;
  } else {
    up_node = nullptr;
  }
 
  for (unsigned int pos : positions) {
    if(not gaps[pos]) {
      find_marginal_at_pos(node, pos, node->left->decendant, right_node, up_node);
      normalize_state_probs(node, pos);
    }
  }
}

// Second recursion.
void SequenceAlignment::update_state_probs(TreeNode* node, unsigned int pos, TreeNode* up_node) {
  // NOTE we can assume up_node is not a nullptr.
  double u = node->SM->get_u();
  double* state_probs = marginal_state_distribution[node->name][pos];

  for(state_element state_j = 0; state_j < (signed char)n_states; state_j++) {
    if(state_probs[state_j] != 0.0) {
      //std::cout << "update: " << (unsigned int)state_j << std::endl;
      state_probs[state_j] *= find_state_prob_given_anc_branch(node->up, state_j, marginal_state_distribution[up_node->name][pos], node, u, pos);
    }
  }
}

void SequenceAlignment::fast_update_state_probs_tips(TreeNode* node, unsigned int pos, TreeNode* up_node) {
  /*
   * Only valid for tips - assumes only up node.
   * Equivilent of:
   * reset_to_base()
   * update_state_probs(node, *pos, node->up->ancestral);
   * NOTE we can assume there is an up node at a tip.
   */

  double u = node->SM->get_u();
  double* state_probs = marginal_state_distribution[node->name][pos];
  double* base_state_probs = prior_state_distribution[node->name][pos];

  for(state_element state_j = 0; state_j < (state_element)n_states; state_j++) {
    if(base_state_probs[state_j] != 0.0) {
      state_probs[state_j] = base_state_probs[state_j] * find_state_prob_given_anc_branch(node->up, state_j, marginal_state_distribution[up_node->name][pos], node, u, pos);
    } else {
      state_probs[state_j] = 0.0;
    }
  }
}

// Third Recursion
int random_state_from_distribution(double* distribution, unsigned int n_states) {
  double r = Random();
  double acc = 0.0;

  for(unsigned int i = 0; i < n_states; i++) {
    acc += distribution[i];

    if(r < acc) return(i);
  }

  assert(acc > 1.0);

  return(n_states-1);
}

state_element SequenceAlignment::pick_state_from_probabilities(TreeNode* node, int pos) {
  /*
   * Picks a state from the marginal posterior distribution (taxa_names_to_state_probs).
   * Also resets the marginal posterior distribution to 0 or 1.
   */
  double* probs = marginal_state_distribution[node->name][pos];

  //state_element e = 0;
  //std::cout << node->name << " " << pos << " ";

  //if(node->up != nullptr) {
  //  e = node->up->ancestral->sequences[domain_name]->at(pos);
  //  std::cout << (signed int)e << " ";
  //}

  double r = Random();
  double acc = 0.0;
  state_element selected_state = -1; // Initially set as gap.

  //state_element top_state = -1;
  //double top_state_prob = 0.0;
  for(unsigned int i = 0; i < n_states; i++) {
    acc += probs[i];

    //if(probs[i] > top_state_prob) {
    //  top_state = probs[i];
    //  top_state_prob = i;
    //}

    if(r < acc and selected_state == -1) {
      selected_state = i;
      probs[i] = 1.0;
    } else { 
      probs[i] = 0.0;
    }
  }

  if (selected_state == -1) {
    std::cout << "Error: unable to select top state." << std::endl;
    exit(EXIT_FAILURE);
  }

  return(selected_state);

  //if(selected_state == -1) {
  //  probs[top_state] = 1.0;
  //  return(top_state);
  //} else {
  //  return(selected_state);
  //}
}

void SequenceAlignment::pick_states_for_node(TreeNode* node, const std::list<unsigned int>& positions) {
  std::vector<bool> gaps = taxa_names_to_gaps[node->name];

  for (const unsigned int pos : positions) {
    // Pick state from marginal distributions.
    if(gaps[pos]) {
      taxa_names_to_sequences[node->name][pos] = -1;
    } else {
      taxa_names_to_sequences[node->name][pos] = pick_state_from_probabilities(node, pos);
    }
  }
}

void SequenceAlignment::reconstruct_expand(const std::list<TreeNode*>& recursion_path, const std::list<unsigned int>& positions) {
  /*
   * This function needs a better name.
   * This recalculates the marginal posteriors of each node and picks sequences, which again alters the posterior
   * distribution.
   * Starts at a randomly chosen node and propagates outwards from there.
   */

  for(auto it = recursion_path.begin(); it != recursion_path.end(); it++) {
    TreeNode* node = *it;
    if(node->isTip()) {
      // Tip Node.
      //reset_to_base(node->name, positions);

      std::vector<bool> gaps = taxa_names_to_gaps[node->name];

      for (unsigned int pos : positions) {
        if(not gaps[pos]) {
          //update_state_probs(node, *pos, node->up->ancestral);
          fast_update_state_probs_tips(node, pos, node->up->ancestral);
          normalize_state_probs(node, pos);
        }
      }
    } else {
      // Internal Node.
      find_state_probs_all(node, positions);
    }

    pick_states_for_node(node, positions);
  }
}

// SAMPLING AND RECURSION
void SequenceAlignment::reverse_recursion(const std::list<unsigned int>& positions) {
  /* 
   * Initial reverse recurstion to start marginal posterior calculations of states at each node.
   * Starts at tips and works up the tree to the root.
   * Nodes are ordered in the list such that they are visted in order up the tree.
   */ 

  for(TreeNode* node : this->tree->nodes()) {
    if(not node->isTip()) {
      find_state_probs_dec_only(node, positions);
    } else {
      // This is important as states at tips can be uncertain.
      reset_to_base(node->name, positions);
    }
  }
}

sample_status SequenceAlignment::sample_with_double_recursion(const std::list<unsigned int>& positions) {
  reverse_recursion(positions);

  // 2nd Recursion - Reverse recursion.
  // Skip first element of reverse list as thats the root - no need to sample second time.
  for (auto n = this->tree->nodes().rbegin(); n != this->tree->nodes().rend(); ++n) {
    TreeNode* node = *n;
    std::vector<bool> gaps = taxa_names_to_gaps[node->name];

    // Reculaculate state probability vector - including up branch.
    TreeNode* up_node = node->up ? node->up->ancestral : nullptr;

    for (unsigned int pos : positions) {
      // Note does not call for root node.
      if((not gaps[pos]) and (up_node != nullptr)) {
        update_state_probs(node, pos, up_node);
        normalize_state_probs(node, pos);
      }
    }

    pick_states_for_node(node, positions);
  }

  return(sample_status({false, true, true}));
}

sample_status SequenceAlignment::sample_with_triple_recursion(const std::list<unsigned int>& positions) {
  const std::list<TreeNode*> nodes = tree->nodes();

  reverse_recursion(positions);

  // 2nd Recursion - Reverse recursion.
  // Skip first element of reverse list as thats the root - no need to sample second time.
  for(auto n = nodes.rbegin(); n != nodes.rend(); ++n) {
    TreeNode* node = *n;
    std::vector<bool> gaps = taxa_names_to_gaps[node->name];

    // Reculaculate state probability vector - including up branch.
    TreeNode* up_node;
    if(node->up) {
      up_node = node->up->ancestral;
    } else {
      up_node = nullptr;
    }

    for(auto pos = positions.begin(); pos != positions.end(); ++pos) {
      // Note does not call for root node.
      if((not gaps[*pos]) and (not (up_node == nullptr))) {
        update_state_probs(node, *pos, up_node);
        normalize_state_probs(node, *pos);
      }

    }
  }

  // 3rd Recursion - picking states.
  reconstruct_expand(tree->get_recursion_path(tree->rand_node()), positions);

  return(sample_status({false, true, true}));
}

// These functions are not critical they are useful though for other user who may not know how they are breaking simPLEX.
// This need to be much more robust - add better error handling.
bool SequenceAlignment::validate(std::list<std::string> seq_names_on_tree, std::map<std::string, SequenceAlignment*> other_alignments) {
  // Chck the sequences on the tree are present in the sequence alignment.
  for(auto n = seq_names_on_tree.begin(); n != seq_names_on_tree.end(); ++n) {
    if(taxa_names_to_sequences.find(*n) == taxa_names_to_sequences.end()) {
      std::cerr << "Error: sequence alignment " << domain_name << " is missing sequence for " << *n << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  // Check gaps match between MSAs.
  for(auto it = other_alignments.begin(); it != other_alignments.end(); ++it) {
    SequenceAlignment* alt_msa = it->second;
    assert(n_columns == alt_msa->n_columns);
    for(auto n = seq_names_on_tree.begin(); n != seq_names_on_tree.end(); ++n) {
      std::vector<bool> gaps = taxa_names_to_gaps[*n];
      std::vector<bool> alt_gaps = alt_msa->taxa_names_to_gaps[*n];
      for(unsigned int i = 0; i < n_columns; i++) {
        if(gaps[i] != alt_gaps[i]) {
          std::cerr << "Error: pattern of gaps do not match in MSAs for sequence " << *n << std::endl;
          exit(EXIT_FAILURE);
        }
      }
    }
  }
  
  return(true);
}

// This is not used currently.
bool SequenceAlignment::match_structure(SequenceAlignment* cmp_msa) {
  for(auto seq = taxa_names_to_sequences.begin(); seq != taxa_names_to_sequences.end(); ++seq) {
    // Check there are corresponding sequences.
    auto it = cmp_msa->taxa_names_to_sequences.find(seq->first);
    if(it != cmp_msa->taxa_names_to_sequences.end()) {
      //Check length of sequence matches.
      if(seq->second.size() != it->second.size()) {
        std::cerr << "Error: in sequence alignment \"" << domain_name << "\": sequences are not the same length as reference." << std::endl;
        exit(EXIT_FAILURE);
        return(false);
      } else {
        for(unsigned int i = 0; i < seq->second.size(); i++) {
          if((seq->second[i] == -1) xor (it->second[i] == -1)) {
            //std::cout << seq->second[i] << " " << it->second[i] << std::endl;
            std::cerr << "Error: in sequence alignment \"" << domain_name << "\" in sequence " << seq->first << " at position " << i << " inconsistant gaps." << std::endl;
            exit(EXIT_FAILURE);
            return(false);
          }
        } 
      }
    } else {
      std::cerr << "Error: in sequence alignment \"" << domain_name << "\": sequence for \"" << seq->first << "\" is not found in reference." << std::endl;
      return(false);
    }
  }
  return(true);
}

SequenceAlignmentParameter::SequenceAlignmentParameter(SequenceAlignment* msa, unsigned int n_sample) : SampleableComponent("SequenceAlignment-" + msa->domain_name) {
  save_count = -1;
  this->msa = msa;
  this->n_sample = n_sample;
  this->n_cols = msa->n_cols();

  // Options
  this->triple_recursion = env.get<bool>("MCMC.triple_recursion");

  // Exit program if invalid environment settings.
  if(n_sample < 1) {
    std::cerr << "Error: MCMC.position_sample_count must be greater than 0." << std::endl;
    exit(EXIT_FAILURE);
  }

  if(n_sample > n_cols) {
    std::cerr << "Error: cannot sample " << n_sample << " from alignment with " << n_cols << " columms." << std::endl;
    std::cerr << "Maximum value of MCMC.position_sample_count is " << n_cols << "." << std::endl;
    exit(EXIT_FAILURE);
  } else if(n_sample == n_cols) {
    this->sample_loc = 0;
  } else {
    this->sample_loc = rand() % n_cols;
  }
}

void SequenceAlignmentParameter::print() {
  std::cout << "SequenceAlignment-" << msa->domain_name << std::endl;
}

std::string SequenceAlignmentParameter::get_type() {
  return("SEQUENCE_ALIGNMENT");
}

sample_status SequenceAlignmentParameter::sample() {
  // Makes list of all positions.
  // Leaving room for a feature where not all positions are sampled each time.

  std::cout << "Sampling " << msa->domain_name << ": "<< sample_loc << "->";

  //Find the positions to be sampled.
  unsigned int last_pos = 0;
  std::list<unsigned int> positions = {};
  while(positions.size() < n_sample) {
    positions.push_back(sample_loc);
    last_pos = sample_loc;

    sample_loc++;
    if(sample_loc >= n_cols) {
      if(not (positions.size() == n_sample)) {
        std::cout << last_pos << ",0->";
      }
      sample_loc = 0;
    }
  }

  std::cout << last_pos << std::endl;

  if(this->triple_recursion) {
    // Triple recursion
    return(msa->sample_with_triple_recursion(positions));
  } else {
    // Double recursion
    return(msa->sample_with_double_recursion(positions));
  }
}

void SequenceAlignmentParameter::undo() {
  std::cerr << "Error: SequenceAlignmentSampling cannot be undone." << std::endl;
  exit(EXIT_FAILURE);
}

void SequenceAlignmentParameter::fix() {
}

void SequenceAlignmentParameter::refresh() {
}

std::string SequenceAlignmentParameter::get_state_header() {
  return(name);
}

std::string SequenceAlignmentParameter::get_state() {
  return("n/a");
}

void SequenceAlignmentParameter::save_to_file(uint128_t gen, double l) {
  save_count += 1;
  msa->saveToFile(save_count, gen, l);
}
