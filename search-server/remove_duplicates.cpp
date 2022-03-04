#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
	std::map<std::set<std::string_view>, int> ids_to_save{};
	int count_id_to_save_next = 0;
	std::set<int> ids_to_remove;
	for (const int document_id: search_server) {
		std::set<std::string_view> words{};
		auto ids_to_find = search_server.GetWordFrequencies(document_id);

		if (!ids_to_find.empty()) {
			for (const auto&[word, freqs] : ids_to_find) {
				words.insert(word);
			}
			ids_to_save[words] = document_id;
			if (ids_to_save.size() > count_id_to_save_next) {
				++count_id_to_save_next;
			} else {
				ids_to_remove.insert(document_id);
			}
		}
	}

	for (int id_to_remove : ids_to_remove) {
		search_server.RemoveDocument(id_to_remove);
		std::cout << "Found duplicate document id: "s << id_to_remove << std::endl;
	}
}