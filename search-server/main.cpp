#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double MAX_DEVIATION = 1e-6;
const double DOCUMENT_RELEVANCE = 0.670599;


template <class Key, class Value>
void Print(ostream& out, const map<Key, Value>& v) {
    bool flag = true;
    for (const auto& [key, value] : v) {
        if (flag == false) {
            out  << ", " << key << ": " << value ;
        } else {
            out << key << ": " << value;
            flag = false;
        }
    }
}

template <class C>
void Print(ostream& out, const C& v) {
    bool flag = true;
    for (const auto& x : v) {
        if (flag == false) {
            out  << ", "  << x ;
        } else {
            out << x;
            flag = false;
        }
    }
}

template<class Key, class Value>
ostream& operator <<(ostream& out, const map<Key, Value>& v) {
    out << '{';
    Print(out, v);
    out << '}';
    return out;
}

template <class T>
ostream& operator <<(ostream& out, const vector<T>& v) {
    out << '[';
    Print(out, v);
    out << ']';
    return out;
}

template<class T>
ostream& operator <<(ostream& out, const set<T>& v) {
    out << '{';
    Print(out, v);
    out << '}';
    return out;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <class Function>
void RunTestImpl(Function function, const string& func) {
    function();
    cerr << func <<  " OK" << endl;
}

#define RUN_TEST(func)  RunTestImpl(func, #func)

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
                           DocumentData{
                                   ComputeAverageRating(ratings),
                                   status
                           });
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus stat = DocumentStatus::ACTUAL) const{
        auto matched_documents = FindTopDocuments(raw_query, [stat](int document_id, DocumentStatus status, int rating) { return status == stat; });
        return matched_documents;
    }

    template<typename Predicate>
    vector<Document> FindTopDocuments(const string& raw_query, Predicate predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < MAX_DEVIATION) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
                text,
                is_minus,
                IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template<typename Predicate>
    vector<Document> FindAllDocuments(const Query& query, Predicate predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (predicate(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                                                document_id,
                                                relevance,
                                                documents_.at(document_id).rating
                                        });
        }
        return matched_documents;
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}
// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

// Тест проверяет добавление документа в поисковую систему
void TestAddingDocumentContent() {
    const vector<int> ratings = {1, 2, 3};

    const int doc_id0 = 0;
    const string content0 = "Good play from person"s;

    const int doc_id1 = 1;
    const string content1 = "different staff and stuff"s;

    const int doc_id2 = 2;
    const string content2 = "football basketball ping-pong"s;

    const int doc_id3 = 3;
    const string content3 = "too easy"s;

    const int doc_id4 = 4;
    const string content4 = "chemistry teacher seats"s;

    {
        SearchServer server;
        const auto first_find_without_adding_documents = server.FindTopDocuments("too football"s);
        ASSERT(first_find_without_adding_documents.empty());
        ASSERT_EQUAL(server.GetDocumentCount(), 0);


        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings);
        const auto second_find_with_two_documents = server.FindTopDocuments("too football"s);
        ASSERT_EQUAL(second_find_with_two_documents.size(), 2);
        ASSERT_EQUAL(server.GetDocumentCount(), 2);


        server.AddDocument(doc_id0, content0, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id4, content4, DocumentStatus::ACTUAL, ratings);
        const auto third_find_with_five_documents = server.FindTopDocuments("too football chemistry different play"s);
        ASSERT_EQUAL(third_find_with_five_documents.size(), 5);
        ASSERT_EQUAL(server.GetDocumentCount(), 5);

    }
}

// Тест проверяет исключение минус-слов из поискового запроса
void TestDocumentContentMinusWords() {
    const vector<int> ratings = {1, 2, 3};

    const int doc_id0 = 0;
    const string content0 = "Good play from person"s;

    const int doc_id1 = 1;
    const string content1 = "different staff and stuff"s;

    const int doc_id2 = 2;
    const string content2 = "football basketball ping-pong"s;

    const int doc_id3 = 3;
    const string content3 = "too easy"s;

    const int doc_id4 = 4;
    const string content4 = "chemistry teacher seats"s;

    {
        SearchServer server;
        server.AddDocument(doc_id0, content0, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id4, content4, DocumentStatus::ACTUAL, ratings);

        const auto found_docs = server.FindTopDocuments("too -easy -chemistry teacher seats play staff basketball"s);
        ASSERT_EQUAL(found_docs.size(), 3);
        ASSERT_EQUAL(found_docs[0].id, doc_id2);
        ASSERT_EQUAL(found_docs[1].id, doc_id0);
        ASSERT_EQUAL(found_docs[2].id, doc_id1);
    }
}

// Tecт проверяет матчинг документов
void TestMatchingDocument() {
    const vector<int> ratings = {1, 2, 3};

    const int doc_id0 = 0;
    const string content0 = "Good play from person"s;

    {
        SearchServer server;
        server.AddDocument(doc_id0, content0, DocumentStatus::ACTUAL, ratings);

        const auto [documents0, status0] = server.MatchDocument("-play from person"s, 0);
        ASSERT_EQUAL(documents0.size(), 0);

        const auto [documents1, status1] = server.MatchDocument("Good play from"s, 0);
        ASSERT_EQUAL(documents1.size(), 3);
        ASSERT_EQUAL(documents1[0], "Good");
        ASSERT_EQUAL(documents1[1], "from");
        ASSERT_EQUAL(documents1[2], "play");

        const auto [documents2, status2] = server.MatchDocument("person Good"s, 0);
        ASSERT_EQUAL(documents2.size(), 2);
        ASSERT_EQUAL(documents2[0], "Good");
        ASSERT_EQUAL(documents2[1], "person");
    }
}

// Tecт проверяет сортировку по релевантности найденных документов
void TestSortRelevanceDocumentContent() {
    const vector<int> ratings = {1, 2, 3};

    const int doc_id0 = 0;
    const string content0 = "Good play too much from person really Good person"s;

    const int doc_id1 = 1;
    const string content1 = "different staff and stuff thor"s;

    const int doc_id2 = 2;
    const string content2 = "play football tree comedy play basketball different ping-pong football"s;

    const int doc_id3 = 3;
    const string content3 = "too comedy comedy different easy tree comedy"s;

    const int doc_id4 = 4;
    const string content4 = "chemistry teacher much different seats comedy"s;
    {
        SearchServer server;
        server.AddDocument(doc_id3, content0, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id4, content1, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id0, content3, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id1, content4, DocumentStatus::ACTUAL, ratings);

        const auto found_docs = server.FindTopDocuments("play football different ping-pong seats"s);

        ASSERT_EQUAL(found_docs.size(), 5);
        ASSERT(found_docs[0].relevance  >= found_docs[1].relevance );
        ASSERT(found_docs[1].relevance  >= found_docs[2].relevance );
        ASSERT(found_docs[2].relevance  >= found_docs[3].relevance );
        ASSERT(found_docs[3].relevance  >= found_docs[4].relevance );
    }
}

// Тест проверяет, корректно ли вычисляется рейтинг документа
void TestDocumentContentRating() {
    const int doc_id0 = 0;
    const string content0 = "Good play too much from person really Good person"s;
    const vector<int> ratings = {2, 4, 6};
    int average_rating = (ratings[0] + ratings[1] + ratings[2]) / ratings.size();
    {
        SearchServer server;
        server.AddDocument(doc_id0, content0, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("Good"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs[0].rating, average_rating);
    }
}

// Тест проверяет работу с функцией предикатом на примере рейтинга
void TestPredicateRating() {
    const int doc_id0 = 0;
    const string content0 = "Good play too much from person really Good person"s;
    const vector<int> ratings0 = {2, 4, 6};

    const int doc_id1 = 1;
    const string content1 = "different staff and stuff thor"s;
    const vector<int> ratings1 = {10, 10, 10};

    const int doc_id2 = 2;
    const string content2 = "play football tree comedy play basketball different ping-pong football"s;
    const vector<int> ratings2 = {20, 20, 20};

    const int doc_id3 = 3;
    const string content3 = "too comedy comedy different easy tree comedy"s;
    const vector<int> ratings3 = {3, 4, 7};
    {
        SearchServer server;
        server.AddDocument(doc_id0, content0, DocumentStatus::ACTUAL, ratings0);
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
        server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);

        const auto found_docs = server.FindTopDocuments("too play different"s, [](int document_id, DocumentStatus status, int rating){
            return rating >= 10;
        });
        ASSERT_EQUAL(found_docs.size(), 2);
        ASSERT_EQUAL(found_docs[0].id, doc_id2);
        ASSERT_EQUAL(found_docs[1].id, doc_id1);
    }
}

// Tecт проверяет нахождение документов с заданным в функции-предикате статусом
void TestDocumentContentPredicate() {
    const vector<int> ratings = {1, 2, 3};

    const int doc_id0 = 0;
    const string content0 = "Good play too much from person really Good person"s;

    const int doc_id1 = 1;
    const string content1 = "different staff and stuff thor"s;

    const int doc_id2 = 2;
    const string content2 = "play football tree comedy play basketball different ping-pong football"s;

    const int doc_id3 = 3;
    const string content3 = "too comedy comedy different easy tree comedy"s;

    const int doc_id4 = 4;
    const string content4 = "chemistry teacher much different seats comedy"s;
    {
        SearchServer server;
        server.AddDocument(doc_id0, content0, DocumentStatus::REMOVED, ratings);
        server.AddDocument(doc_id1, content1, DocumentStatus::BANNED, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id3, content3, DocumentStatus::BANNED, ratings);
        server.AddDocument(doc_id4, content4, DocumentStatus::REMOVED, ratings);
        const auto found_docs = server.FindTopDocuments("different play"s, [](int document_id, DocumentStatus status, int rating){
            return status == DocumentStatus::REMOVED;
        });
        ASSERT_EQUAL(found_docs.size(), 2);
        ASSERT_EQUAL(found_docs[0].id, doc_id0);
        ASSERT_EQUAL(found_docs[1].id, doc_id4);
    }
}

// Тест проверяет правильность вычисления релевантности документа
void TestDocumentContentRelevance() {
    const vector<int> ratings = {1, 2, 3};

    const int doc_id0 = 0;
    const string content0 = "Good play too much from person really Good person everything everything everything"s;

    const int doc_id1 = 1;
    const string content1 = "different staff and stuff thor"s;

    const int doc_id2 = 2;
    const string content2 = "play football tree comedy play basketball different ping-pong football"s;

    const int doc_id3 = 3;
    const string content3 = "too comedy comedy different easy tree comedy"s;

    const int doc_id4 = 4;
    const string content4 = "chemistry teacher much different seats comedy"s;

    double document_count = 5;
    double tf_person_word = (double)2 / (double)12;
    double tf_everything_word = (double)3 / (double)12;
    double idf_document_person = log(document_count / 1);
    double idf_document_everything = log(document_count / 1);
    double document_relevance = tf_person_word * idf_document_person + idf_document_everything * tf_everything_word;
    {
        SearchServer server;
        server.AddDocument(doc_id0, content0, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id4, content4, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("person everything"s);
        ASSERT_EQUAL(found_docs.size(),  1);

        ASSERT(abs(found_docs[0].relevance - document_relevance) < MAX_DEVIATION);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestAddingDocumentContent);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestDocumentContentMinusWords);
    RUN_TEST(TestMatchingDocument);
    RUN_TEST(TestSortRelevanceDocumentContent);
    RUN_TEST(TestDocumentContentRating);
    RUN_TEST(TestPredicateRating);
    RUN_TEST(TestDocumentContentPredicate);
    RUN_TEST(TestDocumentContentRelevance);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}