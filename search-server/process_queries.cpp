#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(
		const SearchServer& search_server,
		const std::vector<std::string>& queries) {
//	LOG_DURATION("\"process_queries.h\": "s);
	std::vector<std::vector<Document>> result(queries.size());
	std::transform(
//			std::execution::par,
			queries.cbegin(), queries.cend(),
			result.begin(),
			[&search_server](const std::string& query) {
				return search_server.FindTopDocuments(query);
			}
	);
	return result;
}

std::vector<Document> ProcessQueriesJoined(
		const SearchServer& search_server,
		const std::vector<std::string>& queries) {
	std::vector<Document> result;
	std::vector<std::vector<Document>> documents = ProcessQueries(search_server, queries);
	for (const std::vector<Document>& document: documents) {
		std::transform(
				//std::execution::par,
				document.cbegin(), document.cend(),
				//result.begin(),
				std::back_inserter(result),
				[](const Document& doc) {
					return doc;
				}
		);
	}
	return result;
}

