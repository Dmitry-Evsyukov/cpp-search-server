#include <algorithm>
#include "process_queries.h"
#include <vector>
#include <execution>
#include <string_view>
#include <map>

using namespace std;

std::vector<std::vector<Document>> ProcessQueries(
        const SearchServer& search_server,
        const std::vector<std::string> queries) {
    vector<vector<Document>> answer(queries.size());
    transform(execution::par, queries.begin(), queries.end(), answer.begin(), [&search_server](string query){
        return search_server.FindTopDocuments(query);
    });
    return answer;
}

list<Document> ProcessQueriesJoined(
        const SearchServer& search_server,
        const std::vector<std::string> queries) {
    auto documents = ProcessQueries(search_server, queries);
    list<Document> answer;
    for (const auto& docs : documents) {
        for (const auto & doc : docs) {
            answer.push_back(doc);
        }
    }
    return answer;
}
