#include "string_processing.h"

std::string ReadLine() {
	std::string s;
	getline(std::cin, s);
	return s;
}

int ReadLineWithNumber() {
	int result;
	std::cin >> result;
	ReadLine();
	return result;
}

//std::vector<std::string> SplitIntoWords(const std::string& text) {
//	std::vector<std::string> words;
//	std::string word;
//	for (const char c: text) {
//		if (c == ' ') {
//			if (!word.empty()) {
//				words.push_back(word);
//				word.clear();
//			}
//		} else {
//			word += c;
//		}
//	}
//	if (!word.empty()) {
//		words.push_back(word);
//	}
//
//	return words;
//}

std::vector<std::string_view> SplitIntoWords(const std::string_view& str) {
    std::vector<std::string_view> result;
    int64_t pos = 0;
    const int64_t pos_end = str.npos;
    while (true) {
        int64_t space = str.find(' ', pos);
        result.push_back(space == pos_end ? str.substr(pos) : str.substr(pos, space - pos));
        if (space == pos_end) {
            break;
        }
        else {
            pos = space + 1;
        }
    }

    return result;
}