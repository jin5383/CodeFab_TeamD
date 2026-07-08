#include "assembler.h"

#include <cctype>
#include <optional>
#include <stdexcept>

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
	const std::string KEYWORD_IMPORT = "import";
	const std::string KEYWORD_ALIAS = "alias";

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
		if (origin == "Class") return makeSimpleToken(TokenType::CLASS, origin);
		if (origin == KEYWORD_IMPORT) return makeSimpleToken(TokenType::IMPORT, origin);
		if (origin == KEYWORD_ALIAS) return makeSimpleToken(TokenType::ALIAS, origin);
		return std::nullopt;
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
		case '<': return makeSimpleToken(TokenType::LESS, "<");
		case '>': return makeSimpleToken(TokenType::GREATER, ">");
		case '{': return makeSimpleToken(TokenType::LEFT_BRACE, "{");
		case '}': return makeSimpleToken(TokenType::RIGHT_BRACE, "}");
		case '.': return makeSimpleToken(TokenType::DOT, ".");
		case ',': return makeSimpleToken(TokenType::COMMA, ",");
		default: return std::nullopt;
		}
	}

	// Assembler::construct의 내부 구현 세부사항. Token List를 소비해 Program 트리를 만든다.
	class Parser
	{
	public:
		explicit Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

		// END_OF_FILE까지 parseStatement()를 반복 호출해 문장 목록 생성
		std::vector<Stmt*> parseStatements()
		{
			std::vector<Stmt*> statements;
			while (getCurrentToken().type != TokenType::END_OF_FILE)
				statements.push_back(parseStatement());
			return statements;
		}

	private:
		const std::vector<Token>& tokens;
		size_t current = 0;

		const Token& getCurrentToken() const { return tokens[current]; } // 커서를 옮기지 않고 미리보기

		// 토큰을 소비하고 커서를 한 칸 옮기기. END_OF_FILE은 넘어가지 않아 범위 밖 접근을 막는다.
		Token getTokenAndAdvance()
		{
			Token token = getCurrentToken();
			if (token.type != TokenType::END_OF_FILE)
				++current;
			return token;
		}

		// 현재 토큰이 기대한 종류면 소비하고 반환, 아니면 구문 오류로 던진다
		Token expectAndAdvance(TokenType type, const std::string& message)
		{
			if (getCurrentToken().type != type)
				throw std::runtime_error(message);
			return getTokenAndAdvance();
		}

		// 토큰 종류에 따라 var 선언/print/블록/if/for/표현식 문장으로 분기
		Stmt* parseStatement()
		{
			switch (getCurrentToken().type)
			{
			case TokenType::VAR: return parseVarDeclStatement();
			case TokenType::PRINT: return parsePrintStatement();
			case TokenType::LEFT_BRACE: return parseBlockStatement();
			case TokenType::IF: return parseIfStatement();
			case TokenType::FOR: return parseForStatement();
			case TokenType::FUNC: throw std::runtime_error("Func statement not yet implemented (Phase 1: Lee)."); // TODO(Lee)
			case TokenType::RETURN: throw std::runtime_error("Return statement not yet implemented (Phase 1: Lee)."); // TODO(Lee)
			case TokenType::CLASS: return parseClassStatement();
			case TokenType::IMPORT: return parseImportStatement();
			default: return parseExpressionStatement();
			}
		}

		Stmt* parseClassStatement()
		{
			getTokenAndAdvance(); // Class
			auto* stmt = new ClassDeclStmt();
			stmt->name = expectAndAdvance(TokenType::IDENTIFIER, "Expect class name.");

			if (getCurrentToken().type == TokenType::COLON)
			{
				getTokenAndAdvance(); // :
				auto* superToken = new Token(expectAndAdvance(TokenType::IDENTIFIER, "Expect superclass name."));
				stmt->superclass = superToken;
			}

			expectAndAdvance(TokenType::LEFT_BRACE, "Expect '{' before class body.");
			while (getCurrentToken().type != TokenType::RIGHT_BRACE &&
				   getCurrentToken().type != TokenType::END_OF_FILE)
			{
				// TODO(Park): 메서드 파싱 - Phase 1에서 구현
				getTokenAndAdvance();
			}
			expectAndAdvance(TokenType::RIGHT_BRACE, "Expect '}' after class body.");
			return stmt;
		}

		// "import "경로" alias 별칭 ;" 을 읽어 ImportStmt를 만든다.
		// 최소 구현: 문법이 올바른지만 검사한다(순환 import, 스코프 규칙 등은 다루지 않음).
		Stmt* parseImportStatement()
		{
			getTokenAndAdvance(); // import
			auto* stmt = new ImportStmt();
			stmt->path = expectAndAdvance(TokenType::STRING, "Expect string literal after 'import'.");
			expectAndAdvance(TokenType::ALIAS, "Expect 'alias' after import path.");
			stmt->alias = expectAndAdvance(TokenType::IDENTIFIER, "Expect alias name after 'alias'.");
			expectAndAdvance(TokenType::SEMICOLON, "Expect ';' after import statement.");
			return stmt;
		}

		// "var 이름 (= 초깃값)? ;" 을 읽어 VarDeclStmt를 만든다
		Stmt* parseVarDeclStatement()
		{
			getTokenAndAdvance(); // var
			auto* stmt = new VarDeclStmt();
			stmt->name = expectAndAdvance(TokenType::IDENTIFIER, "Expect variable name.");

			if (getCurrentToken().type == TokenType::EQUAL)
			{
				getTokenAndAdvance(); // =
				stmt->initializer = parseExpression();
			}

			expectAndAdvance(TokenType::SEMICOLON, "Expect ';' after value.");
			return stmt;
		}

		// "식 ;" 을 ExpressionStmt로 감싸게 구현
		Stmt* parseExpressionStatement()
		{
			auto* stmt = new ExpressionStmt();
			stmt->expression = parseExpression();
			expectAndAdvance(TokenType::SEMICOLON, "Expect ';' after value.");
			return stmt;
		}

		// "print 식 ;" 을 읽어 PrintStmt를 만든다
		Stmt* parsePrintStatement()
		{
			getTokenAndAdvance(); // print
			auto* stmt = new PrintStmt();
			stmt->expression = parseExpression();
			getTokenAndAdvance(); // ;
			return stmt;
		}

		// "{ 문장* }" 을 읽어 BlockStmt를 만든다
		Stmt* parseBlockStatement()
		{
			getTokenAndAdvance(); // {
			auto* block = new BlockStmt();
			while (getCurrentToken().type != TokenType::RIGHT_BRACE && getCurrentToken().type != TokenType::END_OF_FILE)
				block->statements.push_back(parseStatement());
			expectAndAdvance(TokenType::RIGHT_BRACE, "Expect '}' after block.");
			return block;
		}

		// "if ( 조건 ) 문장 (else 문장)?" 을 읽어 IfStmt를 만든다.
		// else는 항상 가장 최근에 열린(재귀 호출 중인) if에 결합되므로 dangling else가 자연히 해결된다.
		Stmt* parseIfStatement()
		{
			getTokenAndAdvance(); // if
			expectAndAdvance(TokenType::LEFT_PAREN, "Expect '(' after 'if'.");
			auto* stmt = new IfStmt();
			stmt->condition = parseExpression();
			getTokenAndAdvance(); // )
			stmt->thenBranch = parseStatement();

			if (getCurrentToken().type == TokenType::ELSE)
			{
				getTokenAndAdvance(); // else
				stmt->elseBranch = parseStatement();
			}

			return stmt;
		}

		// "for ( 초기화? ; 조건? ; 증감? ) 문장" 을 읽어 ForStmt를 만든다
		Stmt* parseForStatement()
		{
			getTokenAndAdvance(); // for
			getTokenAndAdvance(); // (

			auto* stmt = new ForStmt();

			if (getCurrentToken().type == TokenType::SEMICOLON)
				getTokenAndAdvance(); // 초기화 없음: ;
			else if (getCurrentToken().type == TokenType::VAR)
				stmt->init = parseVarDeclStatement(); // 내부에서 ; 까지 소비
			else
				stmt->init = parseExpressionStatement(); // 내부에서 ; 까지 소비

			if (getCurrentToken().type != TokenType::SEMICOLON)
				stmt->condition = parseExpression();
			getTokenAndAdvance(); // ;

			if (getCurrentToken().type != TokenType::RIGHT_PAREN)
				stmt->increment = parseExpression();
			getTokenAndAdvance(); // )

			stmt->body = parseStatement();
			return stmt;
		}

		Expr* parseExpression() { return parseAssignmentExpr(); }

		// "이름 = 식" / "obj.field = 식" 을 읽어 AssignExpr / SetExpr를 만든다. 우결합이므로 재귀로 처리
		Expr* parseAssignmentExpr()
		{
			Expr* expr = parseComparisonExpr();

			if (getCurrentToken().type == TokenType::EQUAL)
			{
				getTokenAndAdvance(); // =
				Expr* value = parseAssignmentExpr();

				if (auto* get = dynamic_cast<GetExpr*>(expr))
				{
					auto* set = new SetExpr();
					set->object = get->object;
					set->name = get->name;
					set->value = value;
					return set;
				}

				auto* variable = dynamic_cast<VariableExpr*>(expr);
				if (variable == nullptr)
					throw std::runtime_error("Invalid assignment target.");

				auto* assign = new AssignExpr();
				assign->name = variable->name;
				assign->value = value;
				return assign;
			}

			return expr;
		}

		// <, > 를 좌결합으로 묶는다. 산술(+ - * /)보다 우선순위가 낮다
		Expr* parseComparisonExpr()
		{
			Expr* left = parseAddSubExpr();

			while (getCurrentToken().type == TokenType::LESS || getCurrentToken().type == TokenType::GREATER)
			{
				Token op = getTokenAndAdvance();
				Expr* right = parseAddSubExpr();

				auto* binary = new BinaryExpr();
				binary->left = left;
				binary->op = op;
				binary->right = right;

				left = binary;
			}

			return left;
		}

		// +, - 를 좌결합으로 묶는다: (10 - 4) - 3
		Expr* parseAddSubExpr()
		{
			Expr* left = parseMulDivExpr();

			while (getCurrentToken().type == TokenType::PLUS || getCurrentToken().type == TokenType::MINUS)
			{
				Token op = getTokenAndAdvance();
				Expr* right = parseMulDivExpr();

				auto* binary = new BinaryExpr();
				binary->left = left;
				binary->op = op;
				binary->right = right;

				left = binary;
			}

			return left;
		}

		// *, / 를 좌결합으로 묶는다. parseAddSubExpr보다 먼저 호출되어 우선순위가 더 높게
		Expr* parseMulDivExpr()
		{
			Expr* left = parseUnaryExpr();

			while (getCurrentToken().type == TokenType::STAR || getCurrentToken().type == TokenType::SLASH)
			{
				Token op = getTokenAndAdvance();
				Expr* right = parseUnaryExpr();

				auto* binary = new BinaryExpr();
				binary->left = left;
				binary->op = op;
				binary->right = right;

				left = binary;
			}

			return left;
		}

		// 단항 마이너스(-a)를 곱셈/나눗셈보다 먼저 결합한다
		Expr* parseUnaryExpr()
		{
			if (getCurrentToken().type == TokenType::MINUS)
			{
				Token op = getTokenAndAdvance();
				auto* unary = new UnaryExpr();
				unary->op = op;
				unary->right = parseUnaryExpr();
				return unary;
			}

			return parsePostfixExpr();
		}

		// Lee(CallExpr/LEFT_PAREN), Park(GetExpr/DOT)이 각자 이 함수 안에 분기를 추가한다 (TODO)
		Expr* parsePostfixExpr()
		{
			Expr* expr = parsePrimary();

			while (true)
			{
				if (getCurrentToken().type == TokenType::LEFT_PAREN)
				{
					getTokenAndAdvance(); // (
					auto* call = new CallExpr();
					call->callee = expr;
					while (getCurrentToken().type != TokenType::RIGHT_PAREN &&
						   getCurrentToken().type != TokenType::END_OF_FILE)
					{
						call->arguments.push_back(parseExpression());
						if (getCurrentToken().type == TokenType::COMMA)
							getTokenAndAdvance();
					}
					expectAndAdvance(TokenType::RIGHT_PAREN, "Expect ')' after arguments.");
					expr = call;
				}
				else if (getCurrentToken().type == TokenType::DOT)
				{
					getTokenAndAdvance(); // .
					Token name = expectAndAdvance(TokenType::IDENTIFIER, "Expect property name after '.'.");
					auto* get = new GetExpr();
					get->object = expr;
					get->name = name;
					expr = get;
				}
				else
				{
					break;
				}
			}

			return expr;
		}

		// 괄호면 GroupingExpr, 식별자면 VariableExpr, 리터럴이면 LiteralExpr, 그 외에는 구문 오류
		Expr* parsePrimary()
		{
			if (getCurrentToken().type == TokenType::LEFT_PAREN)
			{
				getTokenAndAdvance(); // (
				auto* grouping = new GroupingExpr();
				grouping->expression = parseExpression();
				expectAndAdvance(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
				return grouping;
			}

			if (getCurrentToken().type == TokenType::IDENTIFIER)
			{
				auto* variable = new VariableExpr();
				variable->name = getTokenAndAdvance();
				return variable;
			}

			switch (getCurrentToken().type)
			{
			case TokenType::NUMBER:
			case TokenType::STRING:
			case TokenType::TRUE:
			case TokenType::FALSE:
			{
				auto* literal = new LiteralExpr();
				literal->value = getTokenAndAdvance().value;
				return literal;
			}
			default:
				throw std::runtime_error("Expect expression.");
			}
		}
	};
}

std::vector<Token> Assembler::tokenize(const std::string& source) const
{
	std::vector<Token> tokens;
	size_t i = 0;
	while (i < source.size())
	{
		const char c = source[i];

		// 1단계: 공백은 토큰 없이 SKIP
		if (std::isspace(static_cast<unsigned char>(c)))
		{
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
			tokens.push_back(Token{ TokenType::NUMBER, origin, std::stod(origin) });
			continue;
		}

		// 3단계: 알파벳: 연속된 영숫자를 모아 식별자, "var"/"true"/"false" 키워드인지에 따라 토큰 구분
		if (std::isalpha(static_cast<unsigned char>(c)))
		{
			const size_t start = i;
			while (i < source.size() && std::isalnum(static_cast<unsigned char>(source[i])))
				++i;
			const std::string origin = source.substr(start, i - start);
			if (const auto keyword = scanKeywordToken(origin))
				tokens.push_back(*keyword);
			else
				tokens.push_back(makeSimpleToken(TokenType::IDENTIFIER, origin));
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
			tokens.push_back(Token{ TokenType::STRING, origin, origin });
			continue;
		}

		// 5단계: 나머지는 한 글자짜리 연산자/구두점 토큰으로 처리
		if (const auto symbol = scanSymbolToken(c))
			tokens.push_back(*symbol);
		++i;
	}

	// 6단계: 파서가 끝을 알 수 있도록 마지막에 END_OF_FILE 토큰 추가
	tokens.push_back(makeSimpleToken(TokenType::END_OF_FILE, ""));
	return tokens;
}

Program Assembler::construct(const std::vector<Token>& tokens) const
{
	Program program;
	program.statements = Parser(tokens).parseStatements();
	return program;
}

Program Assembler::assemble(const std::string& source) const
{
	return construct(tokenize(source));
}
