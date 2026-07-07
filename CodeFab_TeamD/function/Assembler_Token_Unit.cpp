#include "../function.h"
#include <cctype>
#include <optional>

namespace
{
	Token makeSimpleToken(TokenType type, const std::string& origin)
	{
		return Token{ type, origin, std::monostate{} };
	}

	// 한 글자짜리 연산자/구두점 토큰을 찾아 반환한다. 해당하는 문자가 없으면 nullopt.
	std::optional<Token> scanSymbolToken(char c)
	{
		switch (c)
		{
		case '=': return makeSimpleToken(TokenType::EQUAL, "=");
		case ';': return makeSimpleToken(TokenType::SEMICOLON, ";");
		case '+': return makeSimpleToken(TokenType::PLUS, "+");
		case '-': return makeSimpleToken(TokenType::MINUS, "-");
		case '*': return makeSimpleToken(TokenType::STAR, "*");
		case '/': return makeSimpleToken(TokenType::SLASH, "/");
		case '(': return makeSimpleToken(TokenType::LEFT_PAREN, "(");
		case ')': return makeSimpleToken(TokenType::RIGHT_PAREN, ")");
		default: return std::nullopt;
		}
	}
}

std::vector<Token> tokenizeSource(const std::string& source)
{
	std::vector<Token> tokens;
	size_t i = 0;
	while (i < source.size())
	{
		char c = source[i];

		// 1단계: 공백은 토큰 없이 SKIP
		if (std::isspace(static_cast<unsigned char>(c)))
		{
			++i;
			continue;
		}

		// 2단계: 숫자가 시작되면 연속된 숫자 문자를 끝까지 모아 NUMBER 토큰 하나
		if (std::isdigit(static_cast<unsigned char>(c)))
		{
			size_t start = i;
			while (i < source.size() && std::isdigit(static_cast<unsigned char>(source[i])))
				++i;
			std::string origin = source.substr(start, i - start);
			tokens.push_back(Token{ TokenType::NUMBER, origin, std::stod(origin) });
			continue;
		}

		// 3단계: 알파벳: 연속된 영숫자를 모아 식별자, "var" 키워드인지 아닌지에 따라 VAR/IDENTIFIER 토큰으로 구분
		if (std::isalpha(static_cast<unsigned char>(c)))
		{
			size_t start = i;
			while (i < source.size() && std::isalnum(static_cast<unsigned char>(source[i])))
				++i;
			std::string origin = source.substr(start, i - start);
			TokenType type = TokenType::IDENTIFIER;
			if (origin == "var")
				type = TokenType::VAR;
			tokens.push_back(makeSimpleToken(type, origin));
			continue;
		}

		// 4단계: 나머지는 한 글자짜리 연산자/구두점 토큰으로 처리
		if (auto symbol = scanSymbolToken(c))
			tokens.push_back(*symbol);
		++i;
	}

	// 5단계: 파서가 끝을 알 수 있도록 마지막에 END_OF_FILE 토큰 추가
	tokens.push_back(makeSimpleToken(TokenType::END_OF_FILE, ""));
	return tokens;
}
