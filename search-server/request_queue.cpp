#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server) : search_server_(search_server), empty_results_(0) {}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
    vector<Document> documents = AddFindRequest(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
    return documents;
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
    vector<Document> documents = AddFindRequest(raw_query, DocumentStatus::ACTUAL);
    return documents;
}

int RequestQueue::GetNoResultRequests() const {
    return empty_results_;
}

RequestQueue::QueryResult::QueryResult(int number_, bool result_) : number(number_), result(result_) {}

    



