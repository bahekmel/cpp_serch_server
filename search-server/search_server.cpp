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




// Старый метод
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

//std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(
//	const std::string& raw_query, int document_id) const {
//	const auto query = ParseQuery(raw_query);
//
//	std::vector<std::string> matched_words;
//	for (const std::string& word : query.plus_words) {
//		if (word_to_document_freqs_.count(word) == 0) {
//			continue;
//		}
//		if (word_to_document_freqs_.at(word).count(document_id)) {
//			matched_words.push_back(word);
//		}
//	}
//	for (const std::string& word : query.minus_words) {
//		if (word_to_document_freqs_.count(word) == 0) {
//			continue;
//		}
//		if (word_to_document_freqs_.at(word).count(document_id)) {
//			matched_words.clear();
//			break;
//		}
//	}
//	return { matched_words, documents_.at(document_id).status };
//}

//bool SearchServer::IsStopWord(const std::string& word) const {
//	return stop_words_.count("word"s) > 0;
//}

bool SearchServer::IsStopWord(std::string_view word) const {
	return stop_words_.count(static_cast<std::string>(word)) > 0;
}


//bool SearchServer::IsValidWord(const std::string& word) {
//	return std::none_of(word.begin(), word.end(), [](char c) {
//		return c >= '\0' && c < ' ';
//		});
//}



//bool IsValidWord(const std::string_view word) {
//	return std::none_of(word.begin(), word.end(), [](char c) {
//		return c >= '\0' && c < ' ';
//		});
//}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
	std::vector<std::string> words;
	for (const auto& word : SplitIntoWords(text)) {
		if (!IsValidWord(word)) {


            
			throw std::invalid_argument("Word "s + static_cast<std::string>(word) + " is invalid"s);
		}
		if (!IsStopWord(static_cast<std::string>(word))) {
			words.push_back(static_cast<std::string>(word));
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



//SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string& text) const {
//	if (text.empty()) {
//		throw std::invalid_argument("Query word is empty"s);
//	}
//	std::string word = text;
//	bool is_minus = false;
//	if (word[0] == '-') {
//		is_minus = true;
//		word = word.substr(1);
//	}
//	if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
//		throw std::invalid_argument("Query word "s + text + " is invalid");
//	}
//
//	return { word, is_minus, IsStopWord(word) };
//}

//SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
//	Query result;
//	for (const std::string& word : SplitIntoWords(text)) {
//		const auto query_word = ParseQueryWord(word);
//		if (!query_word.is_stop) {
//			if (query_word.is_minus) {
//				result.minus_words.insert(query_word.data);
//			}
//			else {
//				result.plus_words.insert(query_word.data);
//			}
//		}
//	}
//	return result;
//}








SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string_view text) const {
    std::string word = static_cast<std::string>(text);
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("Query word "s + word + " is invalid");
    }

    return { word, is_minus, IsStopWord(word) };
}

SearchServer::Query SearchServer::ParseQuery(const std::string_view text) const {
    Query result;
    std::vector<std::string_view> words = SplitIntoWords(text);

    std::sort(words.begin(), words.end());
    auto last = std::unique(words.begin(), words.end());
    words.erase(last, words.end());

    for (const auto& word : words) {
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

    //std::sort(result.minus_words.begin(), result.minus_words.end());
    //auto last2 = std::unique(result.minus_words.begin(), result.minus_words.end());
    //result.minus_words.erase(last2, result.minus_words.end());

    //std::sort(result.plus_words.begin(), result.plus_words.end());
    //auto last3 = std::unique(result.plus_words.begin(), result.plus_words.end());
    //result.plus_words.erase(last3, result.plus_words.end());

    return result;
}

SearchServer::Query SearchServer::ParseQuery(std::execution::sequenced_policy seq, std::string_view text) const {
    Query result;
    std::vector<std::string_view> words = SplitIntoWords(text);

    std::sort(seq, words.begin(), words.end());
    auto last = std::unique(seq, words.begin(), words.end());
    words.erase(last, words.end());

    for (const auto& word : words) {
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

    std::sort(seq, result.minus_words.begin(), result.minus_words.end());
    auto last2 = std::unique(seq, result.minus_words.begin(), result.minus_words.end());
    result.minus_words.erase(last2, result.minus_words.end());

    std::sort(seq, result.plus_words.begin(), result.plus_words.end());
    auto last3 = std::unique(seq, result.plus_words.begin(), result.plus_words.end());
    result.plus_words.erase(last3, result.plus_words.end());

    return result;
}
SearchServer::Query SearchServer::ParseQuery(std::execution::parallel_policy par, std::string_view text) const {
    Query result;
    std::vector<std::string_view> words = SplitIntoWords(text);

    std::sort(par, words.begin(), words.end());
    auto last = std::unique(par, words.begin(), words.end());
    words.erase(last, words.end());

    for (const auto& word : words) {
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

    //std::sort(par, result.minus_words.begin(), result.minus_words.end());
    //auto last2 = std::unique(par, result.minus_words.begin(), result.minus_words.end());
    //result.minus_words.erase(last2, result.minus_words.end());

    //std::sort(par, result.plus_words.begin(), result.plus_words.end());
    //auto last3 = std::unique(par, result.plus_words.begin(), result.plus_words.end());
    //result.plus_words.erase(last3, result.plus_words.end());

    return result;
}

SearchServer::MatchDocumentResult SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
   /* {
    LOG_DURATION("parse_query"sv);
    const auto query = ParseQuery(raw_query);
    }*/
    const auto query = ParseQuery(raw_query);
    std::vector<std::string> matched_words;
    for (const auto& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const auto& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }

    //std::sort(matched_words.begin(), matched_words.end());
    //auto last = std::unique(matched_words.begin(), matched_words.end());
    //matched_words.erase(last, matched_words.end());

    return { matched_words, documents_.at(document_id).status };
}


SearchServer::MatchDocumentResult SearchServer::MatchDocument(const std::execution::sequenced_policy& seq, const std::string_view raw_query, int document_id) const
    {
        if (!document_ids_.count(document_id)) {
            using namespace std::literals::string_literals;
            throw std::out_of_range("incorrect document id"s);
        }

        const auto query = std::move(ParseQuery(seq, raw_query));

        if (std::any_of(seq, std::make_move_iterator(query.minus_words.begin()), std::make_move_iterator(query.minus_words.end()),
            [this, document_id](const auto& word) {
                return document_to_word_freqs_.at(document_id).count(word);
            })) {
            return { {}, documents_.at(document_id).status };
        }

        std::vector<std::string> matched_words(query.plus_words.size());
        auto last = std::copy_if(seq, std::make_move_iterator(query.plus_words.begin()), std::make_move_iterator(query.plus_words.end()),
            matched_words.begin(),
            [this, document_id](const auto& word) {
                return document_to_word_freqs_.at(document_id).count(word);
            });

        matched_words.erase(last, matched_words.end());
        return { matched_words, documents_.at(document_id).status };
    }




SearchServer::MatchDocumentResult SearchServer::MatchDocument(const std::execution::parallel_policy& par, const std::string_view raw_query, int document_id) const
    {
    	if (document_ids_.count(document_id) == 0) {
    		using namespace std::literals::string_literals;
    		throw std::out_of_range("document_id incorrect!"s);
    	}
    	const auto query = std::move(ParseQuery(par, raw_query));

    	if (std::any_of(par, std::make_move_iterator(query.minus_words.begin()), std::make_move_iterator(query.minus_words.end()),
    		[this, document_id](const auto& word) {
    			return document_to_word_freqs_.at(document_id).count(word);
    		})) {
    		return { {}, documents_.at(document_id).status };
    	}

    	std::vector<std::string> matched_words(query.plus_words.size());
    	auto last = std::copy_if(par, std::make_move_iterator(query.plus_words.begin()), std::make_move_iterator(query.plus_words.end()),
    		matched_words.begin(),
    		[this, document_id](const auto& word) {
    			return document_to_word_freqs_.at(document_id).count(word);
    		});
    	matched_words.erase(last, matched_words.end());

    	std::sort(par, matched_words.begin(), matched_words.end());
    	auto it = std::unique(par, matched_words.begin(), matched_words.end());
    	return { {matched_words.begin(),it}, SearchServer::documents_.at(document_id).status };

    	//return { matched_words, documents_.at(document_id).status };
    }






double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
	return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

