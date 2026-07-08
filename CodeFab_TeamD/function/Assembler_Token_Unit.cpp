#include "../function.h"
#include <cctype>
#include <optional>

namespace
{
	constexpr char STRING_QUOTE = '"';
	constexpr char DECIMAL_POINT = '.';

	const std::string KEYWORD_VAR = "var";
	const std::string KEYWORD_TRUE = "true";
	const std::string KEYWORD_FALSE = "false";
	const std::string KEYWORD_IF = "if";
	const std::string KEYWORD_ELSE = "else";
	const std::string KEYWORD_FOR = "for";
	const std::string KEYWORD_PRINT = "print";

	Token makeSimpleToken(TokenType type, const std::string& origin)
	{
		return Token{ type, origin, std::monostate{} };
	}

	// 알파벳으로 시작하는 origin이 키워드인지 찾아 반환한다. 키워드가 아니면 nullopt.
	std::optional<Token> scanKeywordToken(const std::string& origin)
	{
		if (origin == KEYWORD_VAR) return makeSimpleToken(TokenType::VAR, origin);
		if (origin == KEYWORD_TRUE) return Token{ TokenType::TRUE, origin, true };
		if (origin == KEYWORD_FALSE) return Token{ TokenType::FALSE, origin, false };
		if (origin == KEYWORD_IF) return makeSimpleToken(TokenType::IF, origin);
		if (origin == KEYWORD_ELSE) return makeSimpleToken(TokenType::ELSE, origin);
		if (origin == KEYWORD_FOR) return makeSimpleToken(TokenType::FOR, origin);
		if (origin == KEYWORD_PRINT) return makeSimpleToken(TokenType::PRINT, origin);
		return std::nullopt;
	}

	// 한 글자짜리 연산자/구두점 토큰을 찾아 반환한다. 해당하는 문자가 없으면 nullopt.
	std::optional<Token> scanSymbolToken(char c)
	{
		switch (c)
		{
		case '=': return makeSimpleToken(TokenType::EQUAL, "=");
		case ';': return makeSimpleToken(TokenType::SEMICOLON, ";");
		case ',': return makeSimpleToken(TokenType::COMMA, ",");
		case '.': return makeSimpleToken(TokenType::DOT, ".");
		case '+': return makeSimpleToken(TokenType::PLUS, "+");
		case '-': return makeSimpleToken(TokenType::MINUS, "-");
		case '*': return makeSimpleToken(TokenType::STAR, "*");
		case '/': return makeSimpleToken(TokenType::SLASH, "/");
		case '(': return makeSimpleToken(TokenType::LEFT_PAREN, "(");
		case ')': return makeSimpleToken(TokenType::RIGHT_PAREN, ")");
		case '<': return makeSimpleToken(TokenType::LESS, "<");
		case '>': return makeSimpleToken(TokenType::GREATER, ">");
		case '{': return makeSimpleToken(TokenType::LEFT_BRACE, "{");
		case '}': return makeSimpleToken(TokenType::RIGHT_BRACE, "}");
		case '[': return makeSimpleToken(TokenType::LEFT_BRACKET, "[");
		case ']': return makeSimpleToken(TokenType::RIGHT_BRACKET, "]");
		default: return std::nullopt;
		}
	}
}

std::vector<Token> tokenizeSource(const std::string& source)
{
	std::vector<Token> tokens;
	size_t i = 0;
	int line = 1;
	while (i < source.size())
	{
		const char c = source[i];

		// 1단계: 공백은 토큰 없이 SKIP. 개행 시 줄 번호 증가
		if (std::isspace(static_cast<unsigned char>(c)))
		{
			if (c == '\n')
				++line;
			++i;
			continue;
		}

		// 2단계: 숫자가 시작되면 연속된 숫자 문자(및 소수점)를 끝까지 모아 NUMBER 토큰 하나
		if (std::isdigit(static_cast<unsigned char>(c)))
		{
			const size_t start = i;
			while (i < source.size() && std::isdigit(static_cast<unsigned char>(source[i])))
				++i;
			const bool isDecimalPoint = i < source.size() && source[i] == DECIMAL_POINT;
			const bool isFollowedByDigit = i + 1 < source.size() && std::isdigit(static_cast<unsigned char>(source[i + 1]));
			if (isDecimalPoint && isFollowedByDigit)
			{
				++i;
				while (i < source.size() && std::isdigit(static_cast<unsigned char>(source[i])))
					++i;
			}
			const std::string origin = source.substr(start, i - start);
			tokens.push_back(Token{ TokenType::NUMBER, origin, std::stod(origin), line });
			continue;
		}

		// 3단계: 알파벳: 연속된 영숫자를 모아 식별자, "var"/"true"/"false" 키워드인지에 따라 토큰 구분
		if (std::isalpha(static_cast<unsigned char>(c)))
		{
			const size_t start = i;
			while (i < source.size() && std::isalnum(static_cast<unsigned char>(source[i])))
				++i;
			const std::string origin = source.substr(start, i - start);
			Token token;
			if (const auto keyword = scanKeywordToken(origin))
				token = *keyword;
			else
				token = makeSimpleToken(TokenType::IDENTIFIER, origin);
			token.line = line;
			tokens.push_back(token);
			continue;
		}

		// 4단계: 문자열 리터럴: 여는 "부터 닫는 "까지(따옴표 제외)를 모아 STRING 토큰 하나
		if (c == STRING_QUOTE)
		{
			const size_t start = ++i;
			while (i < source.size() && source[i] != STRING_QUOTE)
				++i;
			const std::string origin = source.substr(start, i - start);
			if (i < source.size())
				++i; // 닫는 "
			tokens.push_back(Token{ TokenType::STRING, origin, origin, line });
			continue;
		}

		// 5단계: 나머지는 한 글자짜리 연산자/구두점 토큰으로 처리
		if (const auto symbol = scanSymbolToken(c))
		{
			Token token = *symbol;
			token.line = line;
			tokens.push_back(token);
		}
		++i;
	}

	// 6단계: 파서가 끝을 알 수 있도록 마지막에 END_OF_FILE 토큰 추가
	tokens.push_back(Token{ TokenType::END_OF_FILE, "", std::monostate{}, line });
	return tokens;
}
