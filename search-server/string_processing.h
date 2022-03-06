//#pragma once
//
//#include <iostream>
//#include <string>
//#include <vector>
//#include <set>
//
//
//std::string ReadLine();
//
//int ReadLineWithNumber();
//
//std::vector<std::string> SplitIntoWords(const std::string& text);
//
//template<typename StringContainer>
//std::set<std::string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
//	std::set<std::string> non_empty_strings;
//	for (const std::string& str : strings) {
//		if (!str.empty()) {
//			non_empty_strings.insert(str);
//		}
//	}
//	return non_empty_strings;
//}


#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <set>


std::string ReadLine();

int ReadLineWithNumber();

std::vector<std::string_view> SplitIntoWords(std::string_view str);


template<typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
	std::set<std::string, std::less<>> non_empty_strings;
	for (std::string_view str: strings) {
		if (!str.empty()) {
			non_empty_strings.emplace(str);
		}
	}
	return non_empty_strings;
}