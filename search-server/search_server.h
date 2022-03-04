#pragma once

#include "log_duration.h"
#include "read_input_functions.h"
#include "string_processing.h"
#include "document.h"

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

class SearchServer {
public:
	template<typename StringContainer>
	explicit SearchServer(const StringContainer& stop_words)
		: stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
		if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
			throw std::invalid_argument("Some of stop words are invalid"s);
		}
	}

	explicit SearchServer(const std::string& stop_words_text);

	void AddDocument(int document_id, const std::string& document, DocumentStatus status,
		const std::vector<int>& ratings);

	template<typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(const std::string& raw_query,
		DocumentPredicate document_predicate) const {
		const auto query = ParseQuery(raw_query, false);		///skip_sort

		auto matched_documents = FindAllDocuments(query, document_predicate);

		std::sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs,
			const Document& rhs) {
				if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
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

	std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const;

	std::vector<Document> FindTopDocuments(const std::string& raw_query) const;

	int GetDocumentCount() const;

	std::set<int>::const_iterator begin() const;

	std::set<int>::const_iterator end() const;

	void RemoveDocument(int document_id);


	template<typename ExecutionPolicy>
	void RemoveDocument(ExecutionPolicy&& policy, int document_id) {
		if (documents_.count(document_id)) {

			std::map<std::string, double>* word_freqs = &document_to_word_freqs_[document_id];
			std::vector<std::string> words(word_freqs->size());

			std::transform(policy,
				word_freqs->begin(), word_freqs->end(),
				words.begin(),
				[](const auto& item) {
					return item.first;
				});

			std::for_each(policy,
				words.begin(), words.end(),
				[this, document_id](const auto& word) {
					word_to_document_freqs_[word].erase(document_id);
				});


			document_ids_.erase(document_id);
			documents_.erase(document_id);
			document_to_word_freqs_.erase(document_id);

		}
	}



	using MatchDocumentResult = std::tuple<std::vector<std::string>, DocumentStatus>;
	MatchDocumentResult MatchDocument(const std::string& raw_query, int document_id) const;
	MatchDocumentResult MatchDocument(const std::execution::sequenced_policy&, 
									  const std::string& raw_query, int document_id) const;
	MatchDocumentResult MatchDocument(const std::execution::parallel_policy&, 
									  const std::string& raw_query, int document_id) const;



	const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

private:
	struct DocumentData {
		int rating;
		DocumentStatus status;
	};
	const std::set<std::string> stop_words_;
	std::map<std::string, std::map<int, double>> word_to_document_freqs_;
	std::map<int, std::map<std::string, double>> document_to_word_freqs_;
	std::map<int, DocumentData> documents_;
	std::set<int> document_ids_;

	bool IsStopWord(const std::string& word) const;

	static bool IsValidWord(const std::string& word);

	std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

	static int ComputeAverageRating(const std::vector<int>& ratings);

	struct QueryWord {
		std::string data;
		bool is_minus;
		bool is_stop;
	};

	QueryWord ParseQueryWord(const std::string& text) const;

	struct Query {
		std::vector<std::string> plus_words;
		std::vector<std::string> minus_words;
	};

	Query ParseQuery(const std::string& text, bool skip_sort) const;

	double ComputeWordInverseDocumentFreq(const std::string& word) const;

	template<typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
		std::map<int, double> document_to_relevance;
		for (const std::string& word : query.plus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
			for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
				const auto& document_data = documents_.at(document_id);
				if (document_predicate(document_id, document_data.status, document_data.rating)) {
					document_to_relevance[document_id] += term_freq * inverse_document_freq;
				}
			}
		}

		for (const std::string& word : query.minus_words) {
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
};










//#pragma once
//
//#include "log_duration.h"
//#include "read_input_functions.h"
//#include "string_processing.h"
//#include "document.h"
//
//#include <iostream>
//#include <algorithm>
//#include <cmath>
//#include <map>
//#include <set>
//#include <stdexcept>
//#include <string>
//#include <utility>
//#include <vector>
//#include <numeric>
//#include <functional>
//#include <execution>
//#include <list>
//
//using namespace std::string_literals;
//
//
//const int MAX_RESULT_DOCUMENT_COUNT = 5;
//
//class SearchServer {
//public:
//
//	template<typename StringContainer>
//	explicit SearchServer(const StringContainer& stop_words);
//
//	explicit SearchServer(const std::string& stop_words_text);
//	explicit SearchServer(std::string_view stop_words_text);
//	
//
//	void AddDocument(int document_id, std::string_view document, DocumentStatus status,
//														const std::vector<int>& ratings);
//
//	template<typename DocumentPredicate>
//	std::vector<Document> FindTopDocuments(std::string_view raw_query,
//		DocumentPredicate document_predicate) const;
//
//	std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;
//
//	std::vector<Document> FindTopDocuments(std::string_view raw_query) const;
//
//	int GetDocumentCount() const;
//
//	auto begin() const {
//		return document_ids_.begin();
//	}
//
//	auto end() const {
//		return document_ids_.end();
//	}
//
//	void RemoveDocument(int document_id);
//	void RemoveDocument(const std::execution::sequenced_policy&, int document_id);
//	void RemoveDocument(const std::execution::parallel_policy&, int document_id);
//
//	const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;
//
//	using MatchDocumentResult = std::tuple<std::vector<std::string_view>, DocumentStatus>;
//	MatchDocumentResult MatchDocument(std::string_view raw_query, int document_id) const;
//	MatchDocumentResult MatchDocument(const std::execution::sequenced_policy&, 
//									  std::string_view raw_query, int document_id) const;
//	MatchDocumentResult MatchDocument(const std::execution::parallel_policy&, 
//									  std::string_view raw_query, int document_id) const;
//
//
//
//
//private:
//	struct DocumentData {
//		int rating;
//		DocumentStatus status;
//		std::string text;
//	};
//
//	const std::set<std::string, std::less<>> stop_words_;
//
//	std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
//	std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
//	std::map<int, DocumentData> documents_;
//	std::set<int> document_ids_;
//
//	bool IsStopWord(std::string_view word) const;
//
//	static bool IsValidWord(std::string_view word) {
//		return std::none_of(word.begin(), word.end(), [](char c) {
//			return c >= '\0' && c < ' ';
//			});
//	}
//
//	std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;
//
//	static int ComputeAverageRating(const std::vector<int>& ratings);
//
//	struct QueryWord {
//		std::string_view data;
//		bool is_minus;
//		bool is_stop;
//	};
//
//	QueryWord ParseQueryWord(std::string_view text) const;
//
//	struct Query {
//		std::vector<std::string_view> plus_words;
//		std::vector<std::string_view> minus_words;
//	};
//
//	Query ParseQuery(std::string_view text/*, bool skip_sort*/) const;
//
//	double ComputeWordInverseDocumentFreq(std::string_view word) const;
//
//
//	template<typename DocumentPredicate>
//	std::vector<Document> FindAllDocuments(const Query& query, 
//		DocumentPredicate document_predicate) const;
//};
//
//template<typename StringContainer>
//SearchServer::SearchServer(const StringContainer& stop_words)
//	: stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
//	if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
//		throw std::invalid_argument("Some of stop words are invalid"s);
//	}
//}
//
//template<typename DocumentPredicate>
//std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
//	DocumentPredicate document_predicate) const {
//	const auto query = ParseQuery(raw_query);   ///bool skip_sort
//
//	auto matched_documents = FindAllDocuments(query, document_predicate);
//
//	std::sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs,
//		const Document& rhs) {
//			if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
//				return lhs.rating > rhs.rating;
//			}
//			else {
//				return lhs.relevance > rhs.relevance;
//			}
//		});
//	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
//		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
//	}
//
//	return matched_documents;
//}
//
//template<typename DocumentPredicate>
//std::vector<Document> SearchServer::FindAllDocuments(const Query& query, 
//	DocumentPredicate document_predicate) const {
//	std::map<int, double> document_to_relevance;
//	for (const std::string_view word : query.plus_words) {
//		if (word_to_document_freqs_.count(word) == 0) {
//			continue;
//		}
//		const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
//		for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
//			const auto& document_data = documents_.at(document_id);
//			if (document_predicate(document_id, document_data.status, document_data.rating)) {
//				document_to_relevance[document_id] += term_freq * inverse_document_freq;
//			}
//		}
//	}
//
//	for (const std::string_view word : query.minus_words) {
//		if (word_to_document_freqs_.count(word) == 0) {
//			continue;
//		}
//		for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
//			document_to_relevance.erase(document_id);
//		}
//	}
//
//	std::vector<Document> matched_documents;
//	for (const auto [document_id, relevance] : document_to_relevance) {
//		matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
//	}
//	return matched_documents;
//}