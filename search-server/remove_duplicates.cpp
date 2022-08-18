#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    vector<int> id_to_delete;
    map<set<string_view>, int> complect_words;
    for (int document_id : search_server) {
        const auto word_freq = search_server.GetWordFrequencies(document_id);
        set<string_view> words;
        for (const auto [word, freq] : word_freq) {
            words.insert(word);
        }
        if (complect_words.count(words) > 0) {
            id_to_delete.push_back(document_id);
        } else {
            complect_words[words]++;
        }
    }
    for (auto id : id_to_delete) {
        search_server.RemoveDocument(id);
        cout << "Found duplicate document id "s << id << endl;
    }
}