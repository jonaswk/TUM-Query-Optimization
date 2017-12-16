#include "TransformativeAlgos.hpp"

using namespace std;

uint64_t generateClassId(vector<int> relationIDs) {
    uint64_t id = 0;

    for(int i : relationIDs) {
        id = (id << 32 | i);
    }

    return id;
}

//Transformation rules
//Modify directly
//Commutativity: swap left and right sub tree of the join
void commutativity(unique_ptr<JoinTree> join) {
    //The swap
    join->leftSub.swap(join->rightSub);
    //To reduce the number of duplicates, disable T1-T3 for new tree
    //TODO: Disable transformations

}

//Right associativity: requires on level more
void rightAssociativity(unique_ptr<JoinTree> join) {
    //Swap in three steps
    join->leftSub.swap(join->rightSub);
    join->leftSub.swap(join->rightSub->leftSub);
    join->rightSub->leftSub.swap(join->rightSub->rightSub);

    //TODO: Disable/enable transformations
}

//Left associativity: basically the same, just the other way around
void leftAssociativity(unique_ptr<JoinTree> join){
    //Swap
    join->leftSub.swap(join->rightSub);
    join->rightSub.swap(join->leftSub->rightSub);
    join->leftSub->leftSub.swap(join->leftSub->rightSub);

    //TODO: Disable/enable transformations
}



/** Implementation of the tranformative algorithm
 * that makes use of the memo structure
 **/
JoinTree TransformativeAlgos::exhaustiveTrans2(QueryGraph graph, int numberOfRelations) {
    cout << "Starting exhaustive transformation" << endl;
    //1. Initialize MEMO
    cout << "Initializing memo structure " << endl;

    int n = numberOfRelations;

    vector<int> start(n);
    for (int i = 1; i <= n; i++)
    {
        start[i] = i;
    }
    list<int> lt;

    //Generate subsets
    auto subsets = utility::subsets(start,n,1,0,lt);

    for(int i = 2; i<=n;i++) {
        auto s2 = utility::subsets(start,n,i,0,lt);
        subsets.insert(s2.begin(),s2.end());
    }
    //Done


    
    for (auto s : subsets) {
        cout << "{";
        vector<int> v;
        for(int i : s) {
            cout << i+1;
            v.push_back(i+1);
        }
        //Generate the id for the memo class
        uint64_t id = generateClassId(v);
        //Create entry in memo table
        vector<JoinTree> vec;
        memo.insert({{id,vec}});
        cout << "}" << endl;
    }

    //2. ExploreClass
    vector<int> allRel(n);
    iota(allRel.begin(), allRel.end(), 1);
    exploreClass(allRel);


    //3. return minimal join tree from the class
    return JoinTree();
}

/**
 * The exploration method
 * Considers *all* alternatives on one class and applies transformation
**/
void TransformativeAlgos::exploreClass(vector<int> relSetId) {

    cout << "Exploring class: " << endl;
    cout << "{";
    for (int i : relSetId) {
        cout << i << " ";
    }
    cout << "}";
    //while not all trees of C have been explored
    while(0) {
        //1. choose some unexplored tree

        //2. ApplyTransformation2

        //3. Mark T as explored
    }

}
/**
 * Actual application of the transformations
 * Here: RuleSet RS-1 from the lecture
 **/
void TransformativeAlgos::applyTrans(JoinTree theTree) {

    //1. Before doing anything, descend into child trees
    //exploreClass(std::move(theTree.leftSub));
    //exploreClass(std::move(theTree.rightSub));

    //2. for every rule and for every member of child "classes"

    //Rules are implemented as private functions

        //3. Add every tree generated by applying a transformation to MEMO
        // In the class we are currently in

}