#include <iostream>
#include <fstream>
#include "DSString.h"
#include "Tweet.h"
#include <utility>
#include <vector>
#include <map>
#include "WordCounts.h"
#include <iomanip>

using namespace std;

vector<Tweet> load_pairs(const DSString &data, const DSString &target)
{
    auto output = vector<Tweet>();

    cout << "Loading pairs of tweets from " << data << " and " << target << endl;

    char line[4096];
    ifstream raw_tweets(data.c_str());
    if (raw_tweets.is_open()){
        //CSV header, ignore
        raw_tweets.getline(line, 4096);

        while (raw_tweets.getline(line, 4096)){
            DSString str = line;
            vector<DSString *> parts = str.split(',');

            //Some tweets contain commas
            //This would be parts[4],parts[5] etc
            //We can loop over these and join them back
            DSString *text = parts[3];
            for (int i = 4; i < parts.size(); i++){
                *text = *text + *parts[i];
            }

            Tweet tweet = Tweet(parts[3], parts[2], parts[1]);

            //Cleanup this memory, Tweet makes a copy
            for (auto &part : parts){
                delete part;
            }

            output.push_back(tweet);
        }

        raw_tweets.close();
    } else {
        cout << "Error opening " << data << " file" << endl;
        exit(-1);
    }

    ifstream raw_target(target.c_str());
    if (raw_target.is_open()){
        //CSV header, ignore
        raw_target.getline(line, 4096);

        int index = 0;

        //We assume that data and target files include the same tweets
        //in the same order
        while (raw_target.getline(line, 4096)){
            DSString str = line;
            vector<DSString *> parts = str.split(',');

            int classification = parts[1]->atoi();
            output.at(index).SetClassification(classification);
            index += 1;

            for (auto &part : parts){
                delete part;
            }
        }

        raw_target.close();
    } else {
        cout << "Error opening " << target << " file" << endl;
        exit(-1);
    }

    return output;
}

bool filter_tweet(DSString *word)
{
    word->toLower();

    //Check for empty words
    if (*word == ""){
        return false;
    }

    //Links
    if (word->includes("http://")){
        return false;
    }

    if (word->includes("https://")){
        return false;
    }

    //There is a chance our word only was endings
    if (*word == ""){
        return false;
    }

    //Check if its an emoji
    if (!word->isASCII()){
        return false;
    }

    //Filter out letters we dont care about
    word->filter("\"");
    word->filter("&quot;");
    word->filter("&lt;");
    word->filter("&amp");
    word->filter(")");
    word->filter("(");
    word->filter("'");
    word->filter("=");
    word->filter("?");
    word->filter(".");
    word->filter("!");

    //Filter out one letter words
    //These are largely numbers
    return word->length() >= 2;
}

void check_biagram(DSString *word, DSString *next_word)
{
    if (*word == "not"){
        *word = *word + " " + *next_word;
        *next_word = "";
    }

    if (*word == "very"){
        *word = *word + " " + *next_word;
        *next_word = "";
    }
}

WordCounts gen_dict(vector<Tweet> data)
{
    //word -> (positive count, negative count)
    WordCounts word_counts = WordCounts();

    //Loop through every tweet, split it into words
    //Update word count
    for (int i = 0; i < data.size(); i++){
        Tweet tweet = data.at(i);
        vector<DSString *> parts = tweet.GetText()->split(' ');

        word_counts.AddWord(*tweet.GetUser(), tweet.GetClassification());

        for (int j = 0; j < parts.size(); j++){
            DSString *word = parts[j];

            if (!filter_tweet(word)){
                delete parts[j];
                continue;
            };

            if (j + 1 < parts.size()){
                check_biagram(word, parts[j + 1]);
            }

            word_counts.AddWord(*word, tweet.GetClassification());

            //Copy is given to addword
            delete parts[j];
        }

        if (i % 30000 == 0){
            float percent_done = (float)i / data.size() * 100;
            cout << "Trained on " << i << " tweets. Progress: " << int(percent_done) << "%" << endl;
        }
    }

    return word_counts;
}

//Get the words and calc the scores
WordCounts run_training(vector<Tweet> data)
{
    WordCounts words = gen_dict(std::move(data));
    words.GenScores();

    cout << "Ran training on " << words.Size() << " words" << endl;

    return words;
}

//Write output file
void write_errors(vector<Tweet> tweets, float acc, const DSString &path)
{
    ofstream output(path.c_str());

    output << setprecision(3) << fixed;
    output << acc << endl;

    for (int i = 0; i < tweets.size(); i++){
        output << *tweets[i].GetID() << endl;
    }
}

void run_inference(WordCounts weights, vector<Tweet> data, const DSString &output)
{
    auto errors = vector<Tweet>();

    int right = 0;
    int wrong = 0;

    for (int i = 0; i < data.size(); i++){
        Tweet tweet = data.at(i);

        float score = 0;

        //Consider the score of the user
        score += weights.GetScore(*tweet.GetUser());

        vector<DSString *> parts = tweet.GetText()->split(' ');
        for (int j = 0; j < parts.size(); j++){
            DSString *word = parts[j];

            if (!filter_tweet(word)){
                delete parts[j];
                continue;
            };

            if (j + 1 < parts.size()){
                check_biagram(word, parts[j + 1]);
            }

            score += weights.GetScore(*word);
            delete parts[j];
        }

        //Bias to counter dataset
        int prediction = (score / (parts.size() + 1)) > -.03;
        prediction *= 4;

        if (tweet.GetClassification() != prediction){
            errors.push_back(tweet);
            wrong++;
        } else {
            right++;
        }
    }

    float accuracy = (float)right / (right + wrong);
    cout << "Correct: " << right << endl;
    cout << "Incorrect: " << wrong << endl;
    cout << "Total: " << right + wrong << endl;
    cout << "Accuracy: " << accuracy << endl;

    write_errors(errors, accuracy, output);
}

//Main entry loop from main
//Load data, train algo, test algo, write output
void create_algo(const DSString &train_data, const DSString &train_target, const DSString &test_data, const DSString &test_target, const DSString &output)
{
    vector<Tweet> training = load_pairs(train_data, train_target);

    cout << "Done loading training data. " << training.size() << " loaded" << endl;

    vector<Tweet> testing = load_pairs(test_data, test_target);

    cout << "Done loading testing data. " << testing.size() << " loaded" << endl;

    WordCounts weights = run_training(training);

    cout << "Test against testing data" << endl;

    run_inference(weights, testing, output);
}