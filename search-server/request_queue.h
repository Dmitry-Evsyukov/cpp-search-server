#pragma once
#include "search_server.h"
#include <deque>
class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string &raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;
private:
    struct QueryResult {
        QueryResult(int number_, bool result_);
        int number;
        bool result;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_server_;
    int empty_results_;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    std::vector<Document> documents = search_server_.FindTopDocuments(raw_query, document_predicate);
    if (requests_.size() < min_in_day_) {
        if (documents.empty()) {
            ++empty_results_;
            requests_.push_back(QueryResult(requests_.size() + 1, false));
        } else {
            requests_.push_back(QueryResult(requests_.size() + 1, true));
        }
    } else {
        if (documents.empty()) {
            if (requests_.front().result == true) {
                requests_.pop_front();
                ++empty_results_;
                requests_.push_back(QueryResult(requests_.size() + 1, false));
            } else {
                requests_.pop_front();
                requests_.push_back(QueryResult(requests_.size() + 1, false));
            }
        } else {
            if (requests_.front().result == true) {
                requests_.pop_front();
                requests_.push_back(QueryResult(requests_.size() + 1, true));
            } else {
                requests_.pop_front();
                --empty_results_;
                requests_.push_back(QueryResult(requests_.size() + 1, true));
            }
        }
    }

    return documents;
}


