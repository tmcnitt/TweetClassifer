#include "WordCounts.h"
#include "DSString.h"
#include <vector>
#include <numeric>
#include <algorithm>
using namespace std;

WordCounts::WordCounts(){
    pos = vector<int>();
    neg = vector<int>();
    words = vector<DSString>();
    indices = map<DSString, int>();
    scores = vector<float>();
}

void WordCounts::AddWord(DSString word, bool positive){
    try {
        int index = indices.at(word);
        pos[index] += positive;
        neg[index] += !positive;

    } catch(out_of_range){
        
        words.push_back(word);
        pos.push_back(positive);
        neg.push_back(!positive);
        scores.push_back(0);

        pair<DSString, int> cache(word, words.size()-1);
        indices.insert(cache);
    }
}

int WordCounts::Size() const {
    return words.size();
}

void WordCounts::GenScores(){
    for(int i = 0; i < words.size(); i++){
        scores[i] = GenScore(i);
    }
};

float WordCounts::GetScore(DSString word){
    //See if we have this word
    try {

        int index = indices.at(word);
        return scores[index];

    } catch(out_of_range){
        //We dont
        return 0;

    }
}

float WordCounts::GenScore(int index){
    //Get the percentage of times it is used in a postive context
    float ratio_score = (float)(pos[index] - neg[index]) / (pos[index] + neg[index]);

    //Means it was never mentioned in other class, ignore
    if(ratio_score == 1 || ratio_score == -1){
        return 0;
    }

    return ratio_score;
}
