#include "log_duration.h"
#include "test_example_functions.h"
#include "remove_duplicates.h"
#include "search_server.h"
#include "request_queue.h"
#include "paginator.h"
#include "process_queries.h"

#include <execution>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace std;

string GenerateWord(std::mt19937& generator, int max_length) {
    const int length = uniform_int_distribution(1, max_length)(generator);
    std::uniform_int_distribution<int> distribution('a', 'z');
    std::string word(length, ' ');
    for (char& c : word) {
        c = char(distribution(generator));
    }
    return word;
}

vector<string> GenerateDictionary(mt19937& generator, int word_count, int max_length) {
    vector<string> words;
    words.reserve(word_count);
    for (int i = 0; i < word_count; ++i) {
        words.push_back(GenerateWord(generator, max_length));
    }
    words.erase(unique(words.begin(), words.end()), words.end());
    return words;
}

string GenerateQuery(mt19937& generator, const vector<string>& dictionary, int word_count, double minus_prob = 0) {
    string query;
    for (int i = 0; i < word_count; ++i) {
        if (!query.empty()) {
            query.push_back(' ');
        }
        if (uniform_real_distribution<>(0, 1)(generator) < minus_prob) {
            query.push_back('-');
        }
        query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}

vector<string> GenerateQueries(mt19937& generator, const vector<string>& dictionary, int query_count, int max_word_count) {
    vector<string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i) {
        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
    }
    return queries;
}

template <typename ExecutionPolicy>
void Test(string_view mark, const SearchServer& search_server, const vector<string>& queries, ExecutionPolicy&& policy) {
    LOG_DURATION(mark);
    double total_relevance = 0;
    for (const string_view query : queries) {
        for (const auto& document : search_server.FindTopDocuments(policy, query)) {
            total_relevance += document.relevance;
        }
    }
    cout << total_relevance << endl;
}

#define TEST(policy) Test(#policy, search_server, queries, execution::policy)


template <typename ExecutionPolicy>
void Test1(string_view mark, SearchServer search_server, const string& query, ExecutionPolicy&& policy) {
    LOG_DURATION(mark);
    const int document_count = search_server.GetDocumentCount();
    int word_count = 0;
    for (int id = 0; id < document_count; ++id) {
        const auto [words, status] = search_server.MatchDocument(policy, query, id);
        word_count += words.size();
    }
    cout << word_count << endl;
}

#define TEST1(policy) Test1(#policy, search_server, query, execution::policy)

int main() {
    
    {
        SearchServer search_server("and with"s);

        int id = 0;
        for (
            const string& text : {
                
                "white cat and yellow hat"s,
                "curly cat curly tail"s,
                "nasty dog with big eyes"s,
                "nasty pigeon john"s,
            }
            ) {
            search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
        }

        //search_server.RemoveDocument(1);
        
        //search_server.RemoveDocument(std::execution::seq, 1);

        //search_server.RemoveDocument(std::execution::par, 2);
        //search_server.RemoveDocument(std::execution::par, 3);
        //search_server.RemoveDocument(std::execution::par, 4);
        //search_server.RemoveDocument(std::execution::par, 5);

        // Проверка на корректность работы

        cout << "ACTUAL by default:"s << endl;
        //последовательная версия                                     
        for (const Document& document : search_server.FindTopDocuments("curly nasty cat"s)) {
            PrintDocument(document);
        }
        cout << "BANNED:"s << endl;
        //последовательная версия
        for (const Document& document : search_server.FindTopDocuments(execution::seq, "curly nasty cat"s, DocumentStatus::BANNED)) {
            PrintDocument(document);
        }

        cout << "Even ids:"s << endl;
        //параллельная версия
        for (const Document& document : search_server.FindTopDocuments(execution::par, "curly nasty cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
            PrintDocument(document);
        }
    }

     //Проверка на скорость работы
    {
        mt19937 generator;
    
        const auto dictionary = GenerateDictionary(generator, 1000, 10);
        const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);
    
        SearchServer search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i) {
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, { 1, 2, 3 });
        }
    
        const auto queries = GenerateQueries(generator, dictionary, 100, 70);
    
        TEST(seq);
        TEST(par);
    }





     //Предыдущее задание
    {
        SearchServer search_server("and with"s);
    
        int id = 0;
        for (
            const string& text : {
                "funny pet and nasty rat"s,
                "funny pet with curly hair"s,
                "funny pet and not very nasty rat"s,
                "pet with rat and rat and rat"s,
                "nasty rat with curly hair"s,
            }
            ) {
            search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
        }
    
        const string query = "curly and funny -not"s;
    
        {
            const auto [words, status] = search_server.MatchDocument(query, 1);
            cout << words.size() << " words for document 1"s << endl;
            // 1 words for document 1
        }
    
        {
            const auto [words, status] = search_server.MatchDocument(execution::seq, query, 2);
            cout << words.size() << " words for document 2"s << endl;
            // 2 words for document 2
        }
    
        {
            const auto [words, status] = search_server.MatchDocument(execution::par, query, 3);
            cout << words.size() << " words for document 3"s << endl;
            // 0 words for document 3
        }
    }
    
    {
        mt19937 generator;
    
        const auto dictionary = GenerateDictionary(generator, 1000, 10);
        const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);
    
        const string query = GenerateQuery(generator, dictionary, 500, 0.1);
    
        SearchServer search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i) {
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, { 1, 2, 3 });
        }
    
        TEST1(seq);
        TEST1(par);
    }

}

