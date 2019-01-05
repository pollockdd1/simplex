/*
 * TREE PARTS
 * This file holds the classes for the components of the tree:
 * 	- Tree nodes - Contains the sequences.
 * 	- Branche segments - Contains the branch lengths and location of substitutions.
 */

#include "TreeParts.h"
#include "Environment.h"
#include <iostream>

extern double Random();
extern Environment env;

// BRANCH SEGMENT

BranchSegment::BranchSegment(float distance) {
	this->distance = distance;
	rates = std::vector<RateVector*>(env.n, NULL);
	subs = std::vector<substitution>(env.n, {-1, -1, -1});
//	std::cout << "Making new branch segment. Distance: " << this->distance << std::endl;
}

BranchSegment::~BranchSegment() {
	decendant->up = 0;
}

std::ostream& operator<< (std::ostream &out, const BranchSegment &b) {
	out << b.distance;
	return out;
}

// Set rates vector corresponding to position to a particular rate vector.
void BranchSegment::set_rate_vector(int pos) {
	/*
	 * When gap, insert NULL for rate vector for pointer.
	 */
	rates[pos] = NULL;
}

void BranchSegment::set_rate_vector(int pos, RateVector* rv) {
	/*
	 * When non-gap state insert correct pointer to rate vector. 
	 */
	if(rates[pos]) rates[pos]->remove_location(pos, this);
	rates[pos] = rv;
	rv->add_location(pos, this);
}

double BranchSegment::get_rate(int pos, int dec_state) {
	double r = rates[pos]->rates[dec_state]->getValue();
	if(isnan(log(r))) {
		rates[pos]->rates[dec_state]->printValue();
	}
	return(r);
}

// Key Statistics
bool BranchSegment::virtualSubstituionQ(int state) {
	float u = env.u;
	double rate = decendant->SM->selectRateVector(state)->rates[state]->getValue();

	double noSub = 1.0/(1.0 + u*distance);
	double Sub = (rate * distance)/(1 + u*distance);

	noSub = noSub/(Sub+noSub);
	Sub = Sub/(Sub+noSub);

	if(Random() < Sub) {
		return(true);
	} else {
		return(false);
	}
}

void BranchSegment::updateStats() {
	num0subs = 0;
	num1subs = 0;
	subs = std::vector<substitution>(env.n, {-1, -1, -1});

	std::vector<int> anc = *(ancestral->sequence);
	std::vector<int> dec = *(decendant->sequence);
	substitution s;
	for(int pos = 0; pos < anc.size(); pos++) {
		if(dec.at(pos) != -1) {
			if(anc.at(pos) == dec.at(pos)) {
				// Add virtual substitution.
				if(virtualSubstituionQ(anc.at(pos))) {
					num1subs += 1;
					s = {pos, anc.at(pos), dec.at(pos)};
					subs[pos] = s;
				// Don't add virtual substitution.
				} else {
					num0subs += 1;
					s = {-1, -1, -1}; // NULL substitution
					subs[pos] = s;
				}
			} else {
				num1subs += 1;
				s = {pos, anc.at(pos), dec.at(pos)};
				subs[pos] = s;
			}
		}
	}
}

// TREE NODES

int TreeNode::unique_id = 0;

TreeNode::TreeNode(IO::RawTreeNode* raw_tree) {
	/* 
	 * TreeNode Constructor - from rawTreeNode.
	 */
	name = raw_tree->name;
	distance = raw_tree->distance;
	up = 0;
	left = 0;
	right = 0;
	sampled = false;
}

TreeNode::TreeNode(std::string n) {
	/* 
	 * TreeNode Constructor.
	 */
	name = n;
	up = 0;
	left = 0;
	right = 0;
	sampled = false;
}

TreeNode::TreeNode() {
	/* 
	 * TreeNode Constructor - from name and distance.
	 */
	name = "Node" + std::to_string(unique_id);
	unique_id++;
	up = 0;
	left = 0;
	right = 0;
	sampled = false;
}

// Sampling.
void branchLikelihood(double &l, int anc, int dec, float t_b, SubstitutionModel* SM) {
	/*
	 * Calculates the likelihood of a branch, and adds it to the vector l.
	 */
	// Doesn't take into account virtual substitutions.
	float u = env.u;
	if(anc == dec) {
		l *= 1.0/(1.0 + t_b *u);
	} else {
		double rate = SM->selectRateVector(anc)->rates[dec]->getValue();
		l *= (rate*t_b)/(1.0 + t_b *u);
	}
}

std::vector<double> normalizeLikelihoods(std::vector<double> &l) {
	double total = 0.0;
	for(auto it = l.begin(); it != l.end(); ++it) {
		total += *it;
	}

	for(auto it = l.begin(); it != l.end(); ++it) {
		*it = *it / total;
	}

	return(l);
}

void TreeNode::sampleSinglePosition(int pos) {
	int n = env.num_states;	
	std::vector<double> l(n, 1.0);	
	for(int state = 0; state < n; state++) {
		if(up) {
			int anc = up->ancestral->sequence->at(pos);
			if(anc == -1) {
				std::cout << "Gap." << std::endl;
			}
			branchLikelihood(l[state], anc, state, up->distance, SM);
		}

		if(left) {
			int dec = left->decendant->sequence->at(pos);
			if(dec != -1) {
				branchLikelihood(l[state], state, dec, left->distance, SM);
			}
		}

		if(right) {
			int dec = right->decendant->sequence->at(pos);
			if(dec != -1) {
				branchLikelihood(l[state], state, dec, right->distance, SM);
			}
		}
	}

	l = normalizeLikelihoods(l);

	double r = Random();
	int i = 0;
	double c = l[0];
	while(r > c) {
		i++;
		c += l[i];
	}
	

	(*sequence)[pos] = i;

	// Branch rates vector updated.
	RateVector* rv = SM->selectRateVector(i);
	if(left) left->set_rate_vector(pos, rv);	
	if(right) right->set_rate_vector(pos, rv);	
}

void TreeNode::sampleSequence() {
	if(!isTip()) {
		for(int pos = 0; pos < sequence->size(); pos++) {
			// Skip sampling if gap.
			if((*sequence)[pos] != -1) {
				sampleSinglePosition(pos);		
			}
		}
	}
}

bool TreeNode::isTip() {
	if(this->left == 0 and this->right == 0) {
		return(true);
	} else {
		return(false);
	}
}


