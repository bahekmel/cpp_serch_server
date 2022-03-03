#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <set>


std::string ReadLine();

int ReadLineWithNumber();

//std::vector<std::string> SplitIntoWords(const std::string_view& text);

std::vector<std::string_view> SplitIntoWords(const std::string_view& str);





template<typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
	std::set<std::string, std::less<>> non_empty_strings;
	for (const auto& str: strings) {
		if (!str.empty()) {
			non_empty_strings.insert(static_cast<std::string>(str));
		}
	}
	return non_empty_strings;
}






//template <typename StringContainer>
//std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(StringContainer strings) {
//    std::set<std::string, std::less<>> non_empty_strings;
//    //for ( std::string_view str : strings) {
//    for (auto str : strings) {
//        if (!str.empty()) {
//            //non_empty_strings.insert(str.data());
//            non_empty_strings.emplace(std::string(str));
//        }
//    }
//    return non_empty_strings;
//}






//template <typename StringContainer>
//std::set<std::string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
//    std::set<std::string> non_empty_strings;
//    for (const auto& str : strings) {
//        if (!str.empty()) {
//            non_empty_strings.insert(std::string(str));
//        }
//    }
//    return non_empty_strings;
//}