#ifndef Tree_h_
#define Tree_h_

#include <algorithm>
#include <functional>
#include <istream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <list>

#include "Sequence.h"
#include "SubstitutionModel.h"
#include "Trees/TreeParser.h"
#include "TreeParts.h"
#include "BranchSplitting.h"

using std::string;
using std::map;
using std::vector;

class Tree {
	public:
		TreeNode* root;
		SequenceAlignment* MSA;
		SubstitutionModel* SM;

		Tree();
		Tree& operator=(Tree tree);

		int seqLen;
		map<string, vector<int>> names_to_sequences;
		std::list<BranchSegment*> branchList;
		std::list<TreeNode*> nodeList;

		// Internal Tree nodes.
		void connectNodes(TreeNode* &ancestral, BranchSegment* &ancestralBP, TreeNode* &decendant, float distance);
		TreeNode* createTreeNode(IO::RawTreeNode* raw_tree, TreeNode* &ancestralNode, BranchSegment* &ancestralBP);

		// Setting options.
		float max_seg_len; // Max segment length.
		std::function< std::pair<BranchSegment*, BranchSegment*>(float)> splitBranch; // Algorithm for splitting branches.
		
		// Initializing.
		void Initialize(IO::RawTreeNode* raw_tree, SequenceAlignment* &MSA, SubstitutionModel* &SM);

		// Debug tools.
		void printBranchList();
		void printNodeList();
		void printParameters();

		// Sampling.
		virtual void SampleParameters();
		virtual void RecordState();
	
		// Likelihood.
		std::map<float, std::pair<int, int>> findKeyStatistics(); //Find the key statistics need for the likelihood function.
		double calculate_likelihood();
	private:
		void configureSequences(TreeNode* n);
		void configureRateVectors();
		// STP: All these things should be protected, but when they are, the derived 
		// class Tree_B1 seems to not have access to them. The definition of protected
		// here means derived classes should have access to them. 
		//protected:

		float u;
	public:
//		typedef unsigned int size_type;

		static int num_trees;
		int id;
		bool is_constant;


		/* Should this really be _part of_ the tree? Or should the tree simply know
	 	 * about it?
	 	 * Maybe this should be a member of the model. Or perhaps this should
	 	 * simply be a pointer to the integer_to_state.
	 	 *
	 	 * No. The tree should have everything it needs to print itself within
	 	 * itself. It should have all the information in it. That means integer_
		 * to_state must be a member of every tree even though it is identical
	 	 * along all trees.... hm maybe it should be a class static then.
	 	 * Or I could use a shared pointer! That way there is only one matrix. And
	 	 * different instances of trees can have different outputs.
	 	 *
	 	 * This also guarantees that the object will always exist if there is a
	 	 * shared pointer to it (unlike a regular pointer).
	 	 */
 
		//vector<string> states;
		
		/**
	 	 * Should these really be class statics? I like pointers better than class
	 	 * statics. And now since there will be a terminate call, it can delete
	 	 * anything allocated on the heap. So perhaps a better solution would be
	 	 * to allocate the ofstreams on the heap at initialization and then make
	 	 * pointers to them and delete them at termination.
	 	 *
	 	 * Or use shared pointers instead of manually allocating them.
	 	 *
	 	 */

		static std::ofstream tree_out;
		static std::ofstream substitutions_out;
		static std::ofstream sequences_out;

	//	virtual void InitializeSequences(std::map<string, vector<int> > taxa_names_to_sequences);
		void InitializeOutputStreams();

	//	virtual void SampleSubtreeParameters();
	//	void SampleSequence();
	//	void SampleDistance();

	//	virtual void RecordSubtreeState();
	//	void RecordSequence();
	//	void RecordToTreefile();
	//	virtual void RecordSubstitutions();
	//	void RecordChildSubstitutions(Tree* child);
	//	void AddGenerationEndIndicatorsToOutputFiles();

	//	virtual void DescendentStateSampling();
	//	void MetropolisHastingsStateSampling();
};

#endif
