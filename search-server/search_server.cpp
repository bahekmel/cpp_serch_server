#include "search_server.h"


SearchServer::SearchServer(const std::string& stop_words_text)
	: SearchServer(SplitIntoWords(stop_words_text)) {
}

void SearchServer::AddDocument(int document_id, const std::string& document,
	DocumentStatus status, const std::vector<int>& ratings) {
	if ((document_id < 0) || (documents_.count(document_id) > 0)) {
		throw std::invalid_argument("Invalid document_id"s);
	}
	const auto words = SplitIntoWordsNoStop(document);

	const double inv_word_count = 1.0 / words.size();
	for (const std::string& word : words) {
		word_to_document_freqs_[word][document_id] += inv_word_count;
		document_to_word_freqs_[document_id][word] += inv_word_count;
	}
	documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
	document_ids_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query,
	DocumentStatus status) const {
	return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status,
		int rating) {
			return document_status == status;
		});
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {
	return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
	return documents_.size();
}

std::set<int>::const_iterator SearchServer::begin() const {
	return document_ids_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
	return document_ids_.end();
}

void SearchServer::RemoveDocument(int document_id) {
	RemoveDocument(std::execution::seq, document_id);
}

const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const {
	static const std::map<std::string, double> dimmy;
	if (document_to_word_freqs_.count(document_id) == 0) {
		return dimmy;
	}
	return document_to_word_freqs_.at(document_id);
}

SearchServer::MatchDocumentResult SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

///-----------------------------------Авторское---seq---------------------------------///
SearchServer::MatchDocumentResult SearchServer::MatchDocument(const std::execution::sequenced_policy&, 
                                        const std::string& raw_query, int document_id) const {

    if (!document_ids_.count(document_id)) {
        using namespace std::literals::string_literals;
        throw std::out_of_range("incorrect document id"s);
    }

    const auto query = ParseQuery(raw_query, false);        ///skip_sort
    const auto status = documents_.at(document_id).status;

    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return {{}, status};
        }
    }
    std::vector<std::string> matched_words;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    return {matched_words, status};
}
///-----------------------------------Авторское---par---------------------------------///
SearchServer::MatchDocumentResult SearchServer::MatchDocument(const std::execution::parallel_policy& , 
											const std::string& raw_query, int document_id) const {
    	
    if (document_ids_.count(document_id) == 0) {
    		using namespace std::literals::string_literals;
    		throw std::out_of_range("document_id incorrect!"s);
    	}

    const auto query = ParseQuery(raw_query, true);     ///skip_sort
    const auto status = documents_.at(document_id).status;

    const auto word_checker = [this, document_id](const std::string& word) {
        const auto it = word_to_document_freqs_.find(word);
        return it != word_to_document_freqs_.end() && it->second.count(document_id);
    };

    if (std::any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(), 
       word_checker)) {
        return { {}, status };
    }
    std::vector<std::string> matched_words(query.plus_words.size());

    auto words_end = std::copy_if(std::execution::par, query.plus_words.begin(),
    query.plus_words.end(), matched_words.begin(), word_checker);

    std::sort(matched_words.begin(), words_end);

    words_end = std::unique(matched_words.begin(), words_end);

    matched_words.erase(words_end, matched_words.end());
    return {matched_words, status};
}



bool SearchServer::IsStopWord(const std::string& word) const {
	return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string& word) {
	return std::none_of(word.begin(), word.end(), [](char c) {
		return c >= '\0' && c < ' ';
		});
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
	std::vector<std::string> words;
	for (const std::string& word : SplitIntoWords(text)) {
		if (!IsValidWord(word)) {
			throw std::invalid_argument("Word "s + word + " is invalid"s);
		}
		if (!IsStopWord(word)) {
			words.push_back(word);
		}
	}
	return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
	if (ratings.empty()) {
		return 0;
	}
	int rating_sum = 0;
	for (const int rating : ratings) {
		rating_sum += rating;
	}
	return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string& text) const {
	if (text.empty()) {
		throw std::invalid_argument("Query word is empty"s);
	}
	std::string word = text;
	bool is_minus = false;
	if (word[0] == '-') {
		is_minus = true;
		word = word.substr(1);
	}
	if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
		throw std::invalid_argument("Query word "s + text + " is invalid");
	}

	return { word, is_minus, IsStopWord(word) };
}

///-------------------------------Авторское----------------------------------///
SearchServer::Query SearchServer::ParseQuery(const std::string& text, bool skip_sort) const {
    Query result;
    for (const std::string& word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
        	if (query_word.is_minus) {
        		result.minus_words.push_back(query_word.data);
        	} else { 
                result.plus_words.push_back(query_word.data);
        			}
        		}
        	}
    if (!skip_sort) {
        for (auto* words : {&result.plus_words, &result.minus_words}) {
            std::sort(words->begin(), words->end());
            words->erase(unique(words->begin(), words->end()), words->end());
        }
    }
    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
	return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}




//#include "search_server.h"
//
//
//
//SearchServer::SearchServer(std::string_view stop_words_text)
//	: SearchServer(SplitIntoWords(stop_words_text)) {
//}
//
//SearchServer::SearchServer(const std::string& stop_words_text)
//    : SearchServer(std::string_view(stop_words_text)) {
//}
//
//void SearchServer::AddDocument(int document_id, std::string_view document,
//	DocumentStatus status, const std::vector<int>& ratings) {
//
//	if ((document_id < 0) || (documents_.count(document_id) > 0)) {
//		throw std::invalid_argument("Invalid document_id"s);
//	}
//
//    const auto [it, inserted] = documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
//	const auto words = SplitIntoWordsNoStop(it->second.text);
//
//	const double inv_word_count = 1.0 / words.size();
//	for (const std::string_view word : words) {
//		word_to_document_freqs_[word][document_id] += inv_word_count; //static_cast<std::string>(word)
//		document_to_word_freqs_[document_id][word] += inv_word_count;
//	}
//	document_ids_.insert(document_id);
//}
//
//
//
//std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
//	DocumentStatus status) const {
//	return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status,
//		int rating) {
//			return document_status == status;
//		});
//}
//
//std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
//	return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
//}
//
//
//int SearchServer::GetDocumentCount() const {
//	return documents_.size();
//}
//
//
//
//void SearchServer::RemoveDocument(int document_id) {
//	RemoveDocument(std::execution::seq, document_id);
//}
//void SearchServer::RemoveDocument(const std::execution::sequenced_policy&, int document_id) {
//    if (documents_.count(document_id)) {
//
//        std::map<std::string_view, double> word_freqs = std::move(document_to_word_freqs_[document_id]);
//        std::vector<std::string> words(word_freqs.size());
//        std::transform(std::execution::seq,
//            word_freqs.begin(), word_freqs.end(),
//            words.begin(),
//            [](const auto& item) {
//                return std::move(item.first);
//            });
//        std::for_each(std::execution::seq,
//            words.begin(), words.end(),
//            [this, document_id](const auto& word) {
//                word_to_document_freqs_[word].erase(document_id);
//            });
//
//        document_ids_.erase(document_id);
//        documents_.erase(document_id);
//        document_to_word_freqs_.erase(document_id);
//    }
//}
//void SearchServer::RemoveDocument(const std::execution::parallel_policy&, int document_id) {
//    if (documents_.count(document_id)) {
//
//        std::map<std::string_view, double> word_freqs = std::move(document_to_word_freqs_[document_id]);
//        std::vector<std::string> words(word_freqs.size());
//        std::transform(std::execution::par,
//            word_freqs.begin(), word_freqs.end(),
//            words.begin(),
//            [](const auto& item) {
//                return std::move(item.first);
//            });
//        std::for_each(std::execution::par,
//            words.begin(), words.end(),
//            [this, document_id](const auto& word) {
//                word_to_document_freqs_[word].erase(document_id);
//            });
//
//        document_ids_.erase(document_id);
//        documents_.erase(document_id);
//        document_to_word_freqs_.erase(document_id);
//    }
//}
//
//
//
//
//const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
//	static const std::map<std::string_view, double> dimmy;
//	if (document_to_word_freqs_.count(document_id) == 0) {
//		return dimmy;
//	}
//	return document_to_word_freqs_.at(document_id);
//}
//
//
//bool SearchServer::IsStopWord(std::string_view word) const {
//	return stop_words_.count(static_cast<std::string>(word)) > 0;
//}
//
//
//std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
//	std::vector<std::string_view> words;
//	for (const auto& word : SplitIntoWords(text)) {
//		if (!IsValidWord(word)) {
//
//			throw std::invalid_argument("Word "s + static_cast<std::string>(word) + " is invalid"s);
//		}
//		if (!IsStopWord(static_cast<std::string>(word))) {
//			words.push_back(static_cast<std::string>(word));
//		}
//	}
//	return words;
//}
//
//int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
//	if (ratings.empty()) {
//		return 0;
//	}
//	int rating_sum = 0;
//	for (const int rating : ratings) {
//		rating_sum += rating;
//	}
//	return rating_sum / static_cast<int>(ratings.size());
//}
//
//
//SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
//    std::string word = static_cast<std::string>(text);
//    if (word.empty()) {
//        throw std::invalid_argument("Query word is empty"s);
//    }
//    bool is_minus = false;
//    if (word[0] == '-') {
//        is_minus = true;
//        word = word.substr(1);
//    }
//
//    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
//        throw std::invalid_argument("Query word "s + word + " is invalid"s);
//    }
//
//    return {word, is_minus, IsStopWord(word)};
//}
//
//
//
/////-------------------------------Авторское----------------------------------///
//SearchServer::Query SearchServer::ParseQuery(std::string_view text/*, bool skip_sort*/) const {
//    Query result;
//    for (const std::string_view word : SplitIntoWords(text)) {
//        const auto query_word = ParseQueryWord(word);
//        if (!query_word.is_stop) {
//        	if (query_word.is_minus) {
//        		result.minus_words.push_back(query_word.data);
//        	} else { 
//                result.plus_words.push_back(query_word.data);
//        			}
//        		}
//        	}
//    /*if (!skip_sort) {
//        for (auto* words : {&result.plus_words, &result.minus_words}) {
//            std::sort(words->begin(), words->end());
//            words->erase(unique(words->begin(), words->end()), words->end());
//        }
//    }*/
//    return result;
//}
//
//SearchServer::MatchDocumentResult SearchServer::MatchDocument(std::string_view raw_query, int document_id) const {
//    return MatchDocument(std::execution::seq, raw_query, document_id);
//}
//
/////-----------------------------------Авторское---seq---------------------------------///
//SearchServer::MatchDocumentResult SearchServer::MatchDocument(const std::execution::sequenced_policy& seq, 
//                                        std::string_view raw_query, int document_id) const {
//
//    if (!document_ids_.count(document_id)) {
//        using namespace std::literals::string_literals;
//        throw std::out_of_range("incorrect document id"s);
//    }
//
//    using namespace std;
//
//    const auto query = ParseQuery(raw_query);   ///bool skip_sort
//    const auto status = documents_.at(document_id).status;
//    for (const std::string_view word : query.minus_words) {
//        if (word_to_document_freqs_.count(word) == 0) {
//            continue;
//        }
//        if (word_to_document_freqs_.at(word).count(document_id)) {
//            return {{}, status};
//        }
//    }
//    std::vector<std::string_view> matched_words;
//    for (const std::string_view word : query.plus_words) {
//        if (word_to_document_freqs_.count(word) == 0) {
//            continue;
//        }
//        if (word_to_document_freqs_.at(word).count(document_id)) {
//            matched_words.push_back(word);
//        }
//    }
//    return {matched_words, status};
//}
/////-----------------------------------Авторское---par---------------------------------///
//SearchServer::MatchDocumentResult SearchServer::MatchDocument(const std::execution::parallel_policy& par, 
//                                        std::string_view raw_query, int document_id) const {
//    	
//    if (document_ids_.count(document_id) == 0) {
//    		using namespace std::literals::string_literals;
//    		throw std::out_of_range("document_id incorrect!"s);
//    	}
//
//
//    // Замер времени
//    /*{
//        using namespace std;
//        LOG_DURATION("parse_query_par"sv);
//        for (int i = 0; i < 10000; ++i) {
//            const auto query = ParseQuery(raw_query, true);
//        }
//    }*/
//
//    const auto query = ParseQuery(raw_query);   ///bool skip_sort
//    const auto status = documents_.at(document_id).status;
//
//    const auto word_checker = [this, document_id](const std::string_view word) {
//        const auto it = word_to_document_freqs_.find(word);
//        return it != word_to_document_freqs_.end() && it->second.count(document_id);
//    };
//
//    if (std::any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(), 
//       word_checker)) {
//        return { {}, status };
//    }
//    std::vector<std::string_view> matched_words(query.plus_words.size());
//
//    auto words_end = std::copy_if(std::execution::par, query.plus_words.begin(),
//    query.plus_words.end(), matched_words.begin(), word_checker);
//
//    std::sort(matched_words.begin(), words_end);
//
//    words_end = std::unique(matched_words.begin(), words_end);
//
//    matched_words.erase(words_end, matched_words.end());
//    return {matched_words, status};
//}
//
//
//
//double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
//	return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
//}
//
