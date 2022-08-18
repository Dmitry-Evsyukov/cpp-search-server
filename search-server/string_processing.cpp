#include "string_processing.h"
using namespace std;

vector<string_view> SplitIntoWords(string_view text) {
    vector<string_view> words;
    text.remove_prefix(min(text.find_first_not_of(" "), text.size()));

    while (!text.empty()) {
        string_view temp = text.substr(0, text.find(' '));
        words.push_back(temp);
        text.remove_prefix(temp.size());
        text.remove_prefix(min(text.find_first_not_of(" "), text.size()));
    }

    return words;
}