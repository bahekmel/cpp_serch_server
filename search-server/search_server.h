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
#include <future>

const int MAX_RESULT_DOCUMENT_COUNT = 5;

const double ERROR_COMPARSION = 1e-6;

const size_t BUCKETS_NUM = 8;

class SearchServer
{
public:
	template <typename StringContainer>
	explicit SearchServer(const StringContainer& stop_words);

	explicit SearchServer(const std::string&);

	explicit SearchServer(const std::string_view);

	void AddDocument(int, std::string_view, DocumentStatus, const std::vector<int>&);

	template <typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(std::string_view,
		DocumentPredicate) const;
	template <class ExecutionPolicy, typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(ExecutionPolicy&&,
		std::string_view,
		DocumentPredicate) const;

	std::vector<Document> FindTopDocuments(std::string_view, DocumentStatus) const;
	template <class ExecutionPolicy>
	std::vector<Document> FindTopDocuments(ExecutionPolicy&&, std::string_view, DocumentStatus) const;

	std::vector<Document> FindTopDocuments(std::string_view) const;
	template <class ExecutionPolicy>
	std::vector<Document> FindTopDocuments(ExecutionPolicy&&, std::string_view) const;

	int GetDocumentCount() const;

	int GetDocumentId(int) const;

	std::vector<int>::const_iterator begin() const;

	std::vector<int>::const_iterator end() const;

	using MatchDocumentResult = std::tuple<std::vector<std::string_view>, DocumentStatus>;

	MatchDocumentResult MatchDocument(std::string_view raw_query, int document_id) const;

	MatchDocumentResult MatchDocument(const std::execution::sequenced_policy&,
		std::string_view raw_query, int document_id) const;

	MatchDocumentResult MatchDocument(const std::execution::parallel_policy&,
		std::string_view raw_query, int document_id) const;

	void RemoveDocument(int);

	template <class ExecutionPolicy>
	void RemoveDocument(ExecutionPolicy&&, int);

	const std::map<std::string_view, double>& GetWordFrequencies(int) const;

private:
	struct DocumentData {
		int rating;
		DocumentStatus status;
		std::string doc_text;
	};

	struct QueryWord {
		std::string_view data;
		bool is_minus;
		bool is_stop;
	};

	struct Query {
		std::vector<std::string_view> plus_words;
		std::vector<std::string_view> minus_words;
	};

	const std::set<std::string, std::less<>> stop_words_;
	std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
	std::map<int, DocumentData> documents_;
	std::vector<int> document_ids_;

	std::map<int, std::map<std::string_view, double>> document_to_words_;

	bool IsStopWord(std::string_view) const;

	static bool IsValidWord(std::string_view);

	std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view) const;

	static int ComputeAverageRating(const std::vector<int>&);

	QueryWord ParseQueryWord(std::string_view) const;

	Query ParseQuery(std::string_view text, bool skip_sort) const;

	double ComputeWordInverseDocumentFreq(std::string_view word) const;

	template <typename ExecutionPolicy, typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(ExecutionPolicy&& policy, const Query& query, DocumentPredicate document_predicate) const;

	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(const Query&, DocumentPredicate) const;

	template <typename ExecutionPolicy, typename ForwardRange, typename Function>
	void ForEach(const ExecutionPolicy&, ForwardRange&, Function);

	template <typename ForwardRange, typename Function>
	void ForEach(ForwardRange&, Function);
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
	: stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
	using namespace std::string_literals;

	if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
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

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
	return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query) const {
	return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentStatus status) const {
	return FindTopDocuments(policy,
		raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
			return document_status == status;
		});
}

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy&& policy, const Query& query, DocumentPredicate document_predicate) const {

	std::map<int, double> document_to_relevance;
	ConcurrentMap<int, double> concurrent_map(16);
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

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const SearchServer::Query& query,
	DocumentPredicate document_predicate) const {
	return SearchServer::FindAllDocuments(std::execution::seq, query, document_predicate);
}


template <class ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id) {
	const auto& word_freqs = document_to_words_.at(document_id);
	std::vector<const std::string*> words(word_freqs.size());
	std::transform(
		policy,
		word_freqs.begin(), word_freqs.end(),
		words.begin(),
		[](const auto& word) {
			return &word.first;
		});
	std::for_each(policy, words.begin(), words.end(),
		[this, document_id](const std::string* word) {
			word_to_document_freqs_.at(*word).erase(document_id);
		});

	documents_.erase(document_id);
	document_to_words_.erase(document_id);
}