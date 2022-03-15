
/*
#pragma once

#include "log_duration.h"
#include "read_input_functions.h"
#include "string_processing.h"
#include "document.h"
#include "concurrent_map.h"

#include <iostream>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <numeric>
#include <functional>
#include <execution>
#include <list>
#include <mutex>

using namespace std::string_literals;


const int MAX_RESULT_DOCUMENT_COUNT = 5;
const int ERROR_COMPARSION = 1e6;
class SearchServer {
public:

	template<typename StringContainer>
	explicit SearchServer(const StringContainer& stop_words);

	explicit SearchServer(const std::string& stop_words_text);
	explicit SearchServer(std::string_view stop_words_text);


	void AddDocument(int document_id, std::string_view document, DocumentStatus status,
		const std::vector<int>& ratings);


	//=======================================================================//

	// запрос, предикат -> реализация, запрос, предикат
	template<typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(std::string_view raw_query,
		DocumentPredicate document_predicate) const;

	// один поток, запрос, предекат -> реализация
	template <typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query,
		DocumentPredicate document_predicate) const;

	// много поточка, запрос, предекат -> реализация
	template <typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query,
		DocumentPredicate document_predicate) const;

	// один поток, запрос, статус -> реализация, один поток, запрос, предикат
	std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query,
		DocumentStatus status) const;

	// много поточка, запрос, статус  -> реализация, много поточка, запрос, предикат
	std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query,
		DocumentStatus status) const;

	// запрос -> один поток, запрос, статус 
	std::vector<Document> FindTopDocuments(std::string_view raw_query) const;


	//template <class ExecutionPolicy, typename DocumentPredicate>
	//std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query,
	//	DocumentPredicate document_predicate) const;

	//============================================================================//


	int GetDocumentCount() const;

	std::set<int>::const_iterator begin() const;

	std::set<int>::const_iterator end() const;

	void RemoveDocument(int document_id);
	void RemoveDocument(const std::execution::sequenced_policy&, int document_id);
	void RemoveDocument(const std::execution::parallel_policy&, int document_id);

	const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

	using MatchDocumentResult = std::tuple<std::vector<std::string_view>, DocumentStatus>;
	MatchDocumentResult MatchDocument(std::string_view raw_query, int document_id) const;
	MatchDocumentResult MatchDocument(const std::execution::sequenced_policy&,
		std::string_view raw_query, int document_id) const;
	MatchDocumentResult MatchDocument(const std::execution::parallel_policy&,
		std::string_view raw_query, int document_id) const;



private:
	struct DocumentData {
		int rating;
		DocumentStatus status;
		std::string text;
	};

	const std::set<std::string, std::less<>> stop_words_;

	std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
	std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
	std::map<int, DocumentData> documents_;
	std::set<int> document_ids_;

	bool IsStopWord(std::string_view word) const;

	static bool IsValidWord(std::string_view word) {
		return std::none_of(word.begin(), word.end(), [](char c) {
			return c >= '\0' && c < ' ';
			});
	}

	std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

	static int ComputeAverageRating(const std::vector<int>& ratings);

	struct QueryWord {
		std::string_view data;
		bool is_minus;
		bool is_stop;
	};

	QueryWord ParseQueryWord(std::string_view text) const;

	struct Query {
		std::vector<std::string_view> plus_words;
		std::vector<std::string_view> minus_words;
	};

	Query ParseQuery(std::string_view text, bool skip_sort) const;

	double ComputeWordInverseDocumentFreq(std::string_view word) const;



	//==================== FindAllDocuments new ===========================//

	template<typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(const Query& query,
		DocumentPredicate document_predicate) const;

	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const;

	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const;

	//======================================================================//


}; // end SearchServer::





template<typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
	: stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
	if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
		throw std::invalid_argument("Some of stop words are invalid"s);
	}
}


//template <typename DocumentPredicate, typename ExecutionPolicy>
//	std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy&& policy,


	// Реализация один поток, запрос, статус -> один поток, запрос, предикат
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query,
	DocumentStatus status) const {

	return FindTopDocuments(std::execution::seq, raw_query, [status](int document_id, DocumentStatus document_status,
		int rating) {
			return document_status == status;
		});
}

// Реализация много поточка, запрос, статус -> много поточка, запрос, предикат
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query,
	DocumentStatus status) const {
	return FindTopDocuments(std::execution::par, raw_query, [status](int document_id, DocumentStatus document_status,
		int rating) {
			return document_status == status;
		});
}

//================SearchServer::FindTopDocuments new========================//

// Реализация запрос, предикат
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const {
	const SearchServer::Query query = SearchServer::ParseQuery(raw_query, false);
	auto matched_documents = FindAllDocuments(query, document_predicate);
	sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
		if (std::abs(lhs.relevance - rhs.relevance) < ERROR_COMPARSION) {
			return lhs.rating > rhs.rating;
		}
		else {
			return lhs.relevance > rhs.relevance;
		}
		});
	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
	}
	return matched_documents;
}


// Реализация один поток, запрос, предикат
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query,
	DocumentPredicate document_predicate) const {

	const auto query = ParseQuery(raw_query, false);
	auto matched_documents = FindAllDocuments(std::execution::seq, query, document_predicate);
	std::sort(std::execution::seq, matched_documents.begin(), matched_documents.end(),
		[](const Document& lhs, const Document& rhs) {
			return lhs.relevance > rhs.relevance
				|| (std::abs(lhs.relevance - rhs.relevance) < ERROR_COMPARSION && lhs.rating > rhs.rating);
		});
	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
	}
	return matched_documents;

}
// Реализация много поточка, запрос, предикат
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query,
	DocumentPredicate document_predicate) const {

	const auto query = ParseQuery(raw_query, false);
	auto matched_documents = FindAllDocuments(std::execution::par, query, document_predicate);
	std::sort(std::execution::par, matched_documents.begin(), matched_documents.end(),
		[](const Document& lhs, const Document& rhs) {
			return lhs.relevance > rhs.relevance
				|| (std::abs(lhs.relevance - rhs.relevance) < ERROR_COMPARSION && lhs.rating > rhs.rating);
		});
	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
	}
	return matched_documents;

}
//==============================================================================//



//===================SearchServer::FindAllDocuments new=====================//


//template <typename DocumentPredicate, typename ExecutionPolicy>
//std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy&& policy,
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const {

	std::map<int, double> document_to_relevance;
	for (const auto word : query.plus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
		for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
			const auto& document_data = documents_.at(document_id);



			// document_predicate для проверки
			//auto document_predicate_ = ([](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });


			if (document_predicate(document_id, document_data.status, document_data.rating)) {
				document_to_relevance[document_id] += term_freq * inverse_document_freq;
			}
		}
	}

	for (const std::string_view word : query.minus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
			document_to_relevance.erase(document_id);
		}
	}

	std::vector<Document> matched_documents;
	for (const auto [document_id, relevance] : document_to_relevance) {
		matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
	}
	return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy&, const SearchServer::Query& query,
	DocumentPredicate document_predicate) const {


	std::map<int, double> document_to_relevance;
	//  ConcurrentMap<int, double> concurrent_map(std::thread::hardware_concurrency());
	ConcurrentMap<int, double> concurrent_map(4);


	std::for_each(
		std::execution::par,
		query.plus_words.begin(),
		query.plus_words.end(),
		[this, &concurrent_map, &document_predicate](std::string_view word) {
			if (!word_to_document_freqs_.count(word) == 0) {
				const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);

				for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
					const auto& document_data = documents_.at(document_id);
					if (document_predicate(document_id, document_data.status, document_data.rating)) {
						concurrent_map[document_id].ref_to_value += term_freq * inverse_document_freq;
					}
				}

			}
		}
	);

	for_each(
		std::execution::par,
		query.minus_words.begin(),
		query.minus_words.end(),
		[this, &document_predicate, &concurrent_map](std::string_view word) {
			if (!word_to_document_freqs_.count(word) == 0) {
				for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
					concurrent_map.Erase(document_id);
				}
			}
		}
	);

	document_to_relevance = concurrent_map.BuildOrdinaryMap();

	std::vector<Document> matched_documents;
	matched_documents.reserve(document_to_relevance.size());

	for_each(
		std::execution::par,
		document_to_relevance.begin(), document_to_relevance.end(),
		[this, &matched_documents](const auto& document) {
			matched_documents.push_back({
				document.first,
				document.second,
				documents_.at(document.first).rating
				});
		}
	);
	return matched_documents;
}


template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const SearchServer::Query& query, DocumentPredicate document_predicate) const {
	return FindAllDocuments(std::execution::seq, query, document_predicate);
}
*/
