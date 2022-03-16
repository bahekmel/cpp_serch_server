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

//================================================================================//
	// 1
	// Реализация один поток, запрос, предикат
	template <typename ExecutionPolicy, typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(ExecutionPolicy&&, std::string_view raw_query, DocumentPredicate document_predicate) const;
//================================================================================//
	// 2
	template <typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;

	// 3
	template <typename ExecutionPolicy>
	std::vector<Document> FindTopDocuments(ExecutionPolicy&&, std::string_view raw_query) const;
//=================================================================================//
	std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentStatus status) const;

	std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentStatus status) const;

	std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;
//================================================================================//
	// 4
	std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

//================================================================================//
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


	template <typename ExecutionPolicy, typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(ExecutionPolicy&&,
		const SearchServer::Query& query, DocumentPredicate document_predicate) const;

};

template<typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
	: stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
	if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
		throw std::invalid_argument("Some of stop words are invalid"s);
	}
}


template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& police, std::string_view raw_query, DocumentPredicate document_predicate) const {

	bool police_status = std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>;

	const auto query = ParseQuery(raw_query, police_status);
	auto matched_documents = FindAllDocuments(police, query, document_predicate);
	std::sort(police, matched_documents.begin(), matched_documents.end(),
		[](const Document& lhs, const Document& rhs) {
			return lhs.relevance > rhs.relevance
				|| (std::abs(lhs.relevance - rhs.relevance) < ERROR_COMPARSION && lhs.rating > rhs.rating);
		});
	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
	}
	return matched_documents;

}

// 2
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
	return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}
// 3
template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query) const {
	return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}


template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy&& policy, const Query& query, DocumentPredicate document_predicate) const {

	std::map<int, double> document_to_relevance;
	//  ConcurrentMap<int, double> concurrent_map(std::thread::hardware_concurrency());
	ConcurrentMap<int, double> concurrent_map(4);
	std::for_each(policy,
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

	for_each(policy,
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

	for_each(std::execution::seq,
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



//template <typename ExecutionPolicy, typename DocumentPredicate>
//std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy&& policy, const Query& query, DocumentPredicate document_predicate) const {
//
//	if constexpr (std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>) {
//
//		std::map<int, double> document_to_relevance;
//		for (const auto word : query.plus_words) {
//			if (word_to_document_freqs_.count(word) == 0) {
//				continue;
//			}
//			const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
//			for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
//				const auto& document_data = documents_.at(document_id);
//
//				if (document_predicate(document_id, document_data.status, document_data.rating)) {
//					document_to_relevance[document_id] += term_freq * inverse_document_freq;
//				}
//			}
//		}
//
//		for (const std::string_view word : query.minus_words) {
//			if (word_to_document_freqs_.count(word) == 0) {
//				continue;
//			}
//			for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
//				document_to_relevance.erase(document_id);
//			}
//		}
//
//		std::vector<Document> matched_documents;
//		for (const auto [document_id, relevance] : document_to_relevance) {
//			matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
//		}
//		return matched_documents;
//	} else {
//
//			std::map<int, double> document_to_relevance;
//			//ConcurrentMap<int, double> concurrent_map(std::thread::hardware_concurrency());
//			ConcurrentMap<int, double> concurrent_map(4);
//
//
//		std::for_each(std::execution::par,
//			query.plus_words.begin(),
//			query.plus_words.end(),
//			[this, &concurrent_map, &document_predicate](std::string_view word) {
//				if (!word_to_document_freqs_.count(word) == 0) {
//					const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
//
//					for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
//						const auto& document_data = documents_.at(document_id);
//						if (document_predicate(document_id, document_data.status, document_data.rating)) {
//							concurrent_map[document_id].ref_to_value += term_freq * inverse_document_freq;
//						}
//					}
//				}
//			}
//		);
//
//		for_each(std::execution::par,
//			query.minus_words.begin(),
//			query.minus_words.end(),
//			[this, &document_predicate, &concurrent_map](std::string_view word) {
//				if (!word_to_document_freqs_.count(word) == 0) {
//					for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
//						concurrent_map.Erase(document_id);
//					}
//				}
//			}
//		);
//
//		document_to_relevance = concurrent_map.BuildOrdinaryMap();
//
//		std::vector<Document> matched_documents;
//		matched_documents.reserve(document_to_relevance.size());
//
//		for_each(std::execution::par,
//			document_to_relevance.begin(), document_to_relevance.end(),
//			[this, &matched_documents](const auto& document) {
//				matched_documents.push_back({
//					document.first,
//					document.second,
//					documents_.at(document.first).rating
//					});
//			}
//		);
//		return matched_documents;
//	}
//}