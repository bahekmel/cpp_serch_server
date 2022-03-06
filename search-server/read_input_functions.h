//#pragma once
//
//#include "document.h"
//
//#include <iostream>
//#include <string>
//#include <vector>
//
//using namespace std::string_literals;
//
//std::ostream& operator<<(std::ostream& out, const Document& document);
//
//void PrintDocument(const Document& document);
//
//void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words,
//	DocumentStatus status);


#pragma once

#include "document.h"

#include <iostream>
#include <string>
#include <vector>

using namespace std::string_literals;

std::ostream& operator<<(std::ostream& out, const Document& document);

void PrintDocument(const Document& document);

void PrintMatchDocumentResult(int document_id, const std::vector<std::string_view>& words,
							  DocumentStatus status);
