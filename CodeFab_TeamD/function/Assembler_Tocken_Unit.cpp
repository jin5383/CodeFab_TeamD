#include "../function.h"
#include <cctype>
#include <unordered_map>

namespace
{
	const std::unordered_map<std::string, TokenType> keywords = {
		{ "var", TokenType::VAR }, { "if", TokenType::IF }, { "else", TokenType::ELSE },
		{ "for", TokenType::FOR }, { "and", TokenType::AND }, { "or", TokenType::OR },
		{ "print", TokenType::PRINT }, { "true", TokenType::TRUE }, { "false", TokenType::FALSE },
	};

	bool isIdentifierStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
	bool isIdentifierChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }
}

std::vector<Token> tokenizeSource(const std::string& source)
{
	std::vector<Token> tokens;
	size_t pos = 0;

	while (pos < source.size())
	{
		char c = source[pos];

		if (std::isspace(static_cast<unsigned char>(c)))
		{
			++pos;
			continue;
		}
		if (c == '/' && pos + 1 < source.size() && source[pos + 1] == '/')
		{
			while (pos < source.size() && source[pos] != '\n') ++pos;
			continue;
		}
		if (std::isdigit(static_cast<unsigned char>(c)))
		{
			size_t start = pos;
			while (pos < source.size() && (std::isdigit(static_cast<unsigned char>(source[pos])) || source[pos] == '.')) ++pos;
			std::string origin = source.substr(start, pos - start);
			tokens.push_back({ TokenType::NUMBER, origin, std::stod(origin) });
			continue;
		}
		if (isIdentifierStart(c))
		{
			size_t start = pos;
			while (pos < source.size() && isIdentifierChar(source[pos])) ++pos;
			std::string origin = source.substr(start, pos - start);
			auto it = keywords.find(origin);
			TokenType type = (it != keywords.end()) ? it->second : TokenType::IDENTIFIER;
			tokens.push_back({ type, origin, std::monostate{} });
			continue;
		}
		if (c == '"')
		{
			size_t start = ++pos;
			while (pos < source.size() && source[pos] != '"') ++pos;
			std::string value = source.substr(start, pos - start);
			++pos;
			tokens.push_back({ TokenType::STRING, value, value });
			continue;
		}

		switch (c)
		{
		case '(': tokens.push_back({ TokenType::LEFT_PAREN, "(", {} }); break;
		case ')': tokens.push_back({ TokenType::RIGHT_PAREN, ")", {} }); break;
		case '{': tokens.push_back({ TokenType::LEFT_BRACE, "{", {} }); break;
		case '}': tokens.push_back({ TokenType::RIGHT_BRACE, "}", {} }); break;
		case ';': tokens.push_back({ TokenType::SEMICOLON, ";", {} }); break;
		case '+': tokens.push_back({ TokenType::PLUS, "+", {} }); break;
		case '-': tokens.push_back({ TokenType::MINUS, "-", {} }); break;
		case '*': tokens.push_back({ TokenType::STAR, "*", {} }); break;
		case '/': tokens.push_back({ TokenType::SLASH, "/", {} }); break;
		case '=': tokens.push_back({ TokenType::EQUAL, "=", {} }); break;
		case '>': tokens.push_back({ TokenType::GREATER, ">", {} }); break;
		case '<': tokens.push_back({ TokenType::LESS, "<", {} }); break;
		case '!': tokens.push_back({ TokenType::BANG, "!", {} }); break;
		default: break;
		}
		++pos;
	}

	tokens.push_back({ TokenType::END_OF_FILE, "", {} });
	return tokens;
}
