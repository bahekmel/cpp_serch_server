#include <cmath>
#include <numeric>
#include <algorithm>

#include "string_processing.h"
#include "search_server.h"
#include "read_input_functions.h"
#include "log_duration.h"


SearchServer::SearchServer(const std::string& stop_words_text)
	: SearchServer(SplitIntoWords(stop_words_text))
{
}


SearchServer::SearchServer(const std::string_view stop_words_view)
	: SearchServer(SplitIntoWords(stop_words_view))
{
}


void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status,
	const std::vector<int>& ratings) {
	using namespace std::string_literals;

	if ((document_id < 0) || (documents_.count(document_id) > 0)) {
		throw std::invalid_argument("Invalid document_id"s);
	}

	const auto [it, inserted] = documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status, std::string(document) });
	const auto words = SplitIntoWordsNoStop(it->second.doc_text);

	const double inv_word_count = 1.0 / words.size();
	for (std::string_view word : words) {
		word_to_document_freqs_[word][document_id] += inv_word_count;
		document_to_words_[document_id][word] = word_to_document_freqs_[word][document_id];
	}
	document_ids_.push_back(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
	return FindTopDocuments(std::execution::seq,
		raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
			return document_status == status;
		});
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
	return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
	return documents_.size();
}

int SearchServer::GetDocumentId(int index) const {
	return document_ids_.at(index);
}

std::vector<int>::const_iterator SearchServer::begin() const {
	return document_ids_.begin();
}

std::vector<int>::const_iterator SearchServer::end() const {
	return document_ids_.end();
}

SearchServer::MatchDocumentResult SearchServer::MatchDocument(std::string_view raw_query, int document_id) const {
	return MatchDocument(std::execution::seq, raw_query, document_id);
}

SearchServer::MatchDocumentResult SearchServer::MatchDocument(const std::execution::sequenced_policy&,
	std::string_view raw_query, int document_id) const {

	if (!documents_.count(document_id)) {
		using namespace std::literals::string_literals;
		throw std::out_of_range("incorrect document id"s);
	}
	using namespace std;
	bool skip_sort = false;
	const auto query = ParseQuery(raw_query, skip_sort);
	const auto status = documents_.at(document_id).status;
	for (const std::string_view word : query.minus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		if (word_to_document_freqs_.at(word).count(document_id)) {
			return { {}, status };
		}
	}
	std::vector<std::string_view> matched_words;
	for (const std::string_view word : query.plus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		if (word_to_document_freqs_.at(word).count(document_id)) {
			matched_words.push_back(word);
		}
	}
	return { matched_words, status };
}

SearchServer::MatchDocumentResult SearchServer::MatchDocument(const std::execution::parallel_policy&,
	std::string_view raw_query, int document_id) const {

	if (documents_.count(document_id) == 0) {
		using namespace std::literals::string_literals;
		throw std::out_of_range("document_id incorrect!"s);
	}
	bool skip_sort = true;
	const auto query = ParseQuery(raw_query, skip_sort);
	const auto status = documents_.at(document_id).status;

	const auto word_checker = [this, document_id](const std::string_view word) {
		const auto it = word_to_document_freqs_.find(word);
		return it != word_to_document_freqs_.end() && it->second.count(document_id);
	};

	if (std::any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(),
		word_checker)) {
		return { {}, status };
	}
	std::vector<std::string_view> matched_words(query.plus_words.size());

	auto words_end = std::copy_if(std::execution::par, query.plus_words.begin(),
		query.plus_words.end(), matched_words.begin(), word_checker);

	std::sort(matched_words.begin(), words_end);
	words_end = std::unique(matched_words.begin(), words_end);
	matched_words.erase(words_end, matched_words.end());
	return { matched_words, status };
}

void SearchServer::RemoveDocument(int document_id)
{
	if (documents_.count(document_id) == 0) {
		return;
	}
	documents_.erase(document_id);
	auto new_end_it = std::remove(document_ids_.begin(), document_ids_.end(), document_id);
	document_ids_.erase(new_end_it, document_ids_.end());

	for (auto& [word, submap] : word_to_document_freqs_) {
		if (submap.count(document_id) > 0) {
			submap.erase(document_id);
		}
	}
	document_to_words_.erase(document_id);
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const
{
	static std::map<std::string_view, double> word_freqs_;
	word_freqs_.clear();

	if (documents_.count(document_id) > 0) {
		word_freqs_ = document_to_words_.at(document_id);
	}
	return word_freqs_;
}

bool SearchServer::IsStopWord(std::string_view word) const {
	return stop_words_.count(word) > 0;
}


bool SearchServer::IsValidWord(std::string_view word) {
	return std::none_of(word.begin(), word.end(), [](char c) {
		return c >= '\0' && c < ' ';
		});
}


std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
	using namespace std::string_literals;

	std::vector<std::string_view> words;
	for (std::string_view word : SplitIntoWords(text)) {
		if (!IsValidWord(word)) {
			throw std::invalid_argument("Word "s + word.data() + " is invalid"s);
		}
		if (!IsStopWord(word)) {
			words.push_back(word);
		}
	}
	return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
	int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
	return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
	using namespace std::string_literals;

	if (text.empty()) {
		throw std::invalid_argument("Query word is empty"s);
	}
	std::string_view word = text;
	bool is_minus = false;
	if (word[0] == '-') {
		is_minus = true;
		word = word.substr(1);
	}
	if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
		throw std::invalid_argument("Query word "s + text.data() + " is invalid"s);
	}
	return { word, is_minus, IsStopWord(word) };
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text, bool skip_sort) const {
	Query result;

	for (const std::string_view word : SplitIntoWords(text)) {
		const auto query_word = ParseQueryWord(word);
		if (!query_word.is_stop) {
			if (query_word.is_minus) {
				result.minus_words.push_back(query_word.data);
			}
			else {
				result.plus_words.push_back(query_word.data);
			}
		}
	}

	if (!skip_sort) {
		for (auto* words : { &result.plus_words, &result.minus_words }) {
			std::sort(words->begin(), words->end());
			words->erase(unique(words->begin(), words->end()), words->end());
		}
	}
	return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
	return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}