#include "search_server.h"
#include <cmath>
#include <numeric>
#include <iterator>

using namespace std;

SearchServer::SearchServer(const string& stop_words_text) : SearchServer(string_view(stop_words_text))  {}

SearchServer::SearchServer(const std::string_view stop_words_text) : SearchServer(SplitIntoWords(stop_words_text)) {}

void SearchServer::AddDocument(int document_id, const string_view document, DocumentStatus status, const vector<int>& ratings) {
    if ((document_id < 0) || (document_ids_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (const string_view word : words) {
        document_to_word_freqs_[document_id][string(word)] += inv_word_count;
        word_to_document_freqs_[document_to_word_freqs_[document_id].find(word)->first][document_id] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static map<string_view, double> empty_answer;
    if (document_to_word_freqs_.count(document_id) == 0) {
        return empty_answer;
    }

    return (map<string_view, double>&) document_to_word_freqs_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(execution::seq, document_id);
}

void SearchServer::RemoveDocument(std::execution::sequenced_policy, int document_id) {
    if (documents_.count(document_id) == 0) {
        return;
    }
    documents_.erase(document_id);
    auto& word_freq = document_to_word_freqs_.at(document_id);
    for (auto [word, freq] : word_freq) {
        word_to_document_freqs_[word].erase(document_id);
    }
    document_to_word_freqs_.erase(document_id);
    document_ids_.erase(document_id);
}

void SearchServer::RemoveDocument(execution::parallel_policy, int document_id) {
    if (document_ids_.count(document_id) == 0) {
        throw invalid_argument("invalid document id");
    }

    documents_.erase(document_id);

    map<string, double, less<>>& word_freq = document_to_word_freqs_.at(document_id);
    vector<const string*> temp;
    temp.reserve(word_freq.size());

    for (auto iter = word_freq.begin(); iter != word_freq.end(); ++iter) {
        temp.push_back((&iter->first));
    }

    for_each(execution::par, temp.begin(), temp.end(),  [&](const string* word){
        word_to_document_freqs_[*word].erase(document_id);
    });

    document_to_word_freqs_.erase(document_id);

    document_ids_.erase(document_id);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view raw_query, int document_id) const {
    const auto query = ParseQuery(execution::seq, raw_query);

    vector<string_view> matched_words;

    for (const auto word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            return {matched_words, documents_.at(document_id).status};
        }
    }

    for (const auto word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    sort(matched_words.begin(), matched_words.end());
    return {matched_words, documents_.at(document_id).status};
}

std::tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy policy, const std::string_view raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}

std::tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy policy, const std::string_view raw_query, int document_id) const {
    const Query query = ParseQuery(policy, raw_query);

    vector<string_view> matched_words;

    if (any_of(policy, query.minus_words.begin(), query.minus_words.end(), [this, document_id] (auto &word) {
        return word_to_document_freqs_.count(word) != 0 && word_to_document_freqs_.at(word).count(document_id);})) {
        matched_words.clear();
        return {matched_words, documents_.at(document_id).status};
    }

    matched_words.resize(query.plus_words.size());
    auto new_end = copy_if(policy, query.plus_words.begin(), query.plus_words.end(),matched_words.begin(),
                           [&](const auto& plus_word) {
                               return word_to_document_freqs_.count(plus_word) != 0 && word_to_document_freqs_.at(plus_word).count(document_id);
                           });


    matched_words.resize(distance(matched_words.begin(), new_end));

    sort(policy, matched_words.begin(), matched_words.end());
    new_end = unique(policy, matched_words.begin(), matched_words.end());
    matched_words.resize(distance(matched_words.begin(), new_end));

    return {matched_words, documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(const string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const string_view word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view text) const {
    vector<string_view> words;
    for (const string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + string{word} + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
        throw invalid_argument("Query word "s + string{text} + " is invalid");
    }

    return {text, is_minus, IsStopWord(text)};
}

SearchServer::Query SearchServer::ParseQuery(const std::string_view text) const {
    return ParseQuery(std::execution::seq, text);
}


SearchServer::Query SearchServer::ParseQuery(std::execution::sequenced_policy policy, const string_view text) const {
    Query result;
    for (const auto word: SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            } else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    sort(result.plus_words.begin(), result.plus_words.end());
    result.plus_words.erase(unique(result.plus_words.begin(), result.plus_words.end()), result.plus_words.end());

    sort(result.minus_words.begin(), result.minus_words.end());
    result.minus_words.erase(unique(result.minus_words.begin(), result.minus_words.end()), result.minus_words.end());

    return result;
}

SearchServer::Query SearchServer::ParseQuery(std::execution::parallel_policy policy, const string_view text) const {
    Query result;
    for (const auto word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            } else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}


