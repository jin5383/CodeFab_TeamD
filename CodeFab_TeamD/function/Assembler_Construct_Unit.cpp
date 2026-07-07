#include "../function.h"

namespace
{
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
		Token getTokenAndAdvance() { return tokens[current++]; } // 토큰을 소비하고 커서를 한 칸 옮기기

		// VAR로 시작하면 변수 선언, 아니면 표현식 문장
		Stmt* parseStatement()
		{
			if (getCurrentToken().type == TokenType::VAR)
				return parseVarDeclStatement();
			return parseExpressionStatement();
		}

		// "var 이름 (= 초깃값)? ;" 을 읽어 VarDeclStmt를 만든다
		Stmt* parseVarDeclStatement()
		{
			getTokenAndAdvance(); // var
			auto* stmt = new VarDeclStmt();
			stmt->name = getTokenAndAdvance(); // 변수 이름

			if (getCurrentToken().type == TokenType::EQUAL)
			{
				getTokenAndAdvance(); // =
				stmt->initializer = parseExpression();
			}

			getTokenAndAdvance(); // ;
			return stmt;
		}

		// "식 ;" 을 ExpressionStmt로 감싸게 구현
		Stmt* parseExpressionStatement()
		{
			auto* stmt = new ExpressionStmt();
			stmt->expression = parseExpression();
			getTokenAndAdvance(); // ;
			return stmt;
		}

		Expr* parseExpression() { return parseAddSubExpr(); }

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
			Expr* left = parsePrimary();

			while (getCurrentToken().type == TokenType::STAR || getCurrentToken().type == TokenType::SLASH)
			{
				Token op = getTokenAndAdvance();
				Expr* right = parsePrimary();

				auto* binary = new BinaryExpr();
				binary->left = left;
				binary->op = op;
				binary->right = right;

				left = binary;
			}

			return left;
		}

		// 괄호면 GroupingExpr, 아니면 숫자 리터럴
		Expr* parsePrimary()
		{
			if (getCurrentToken().type == TokenType::LEFT_PAREN)
			{
				getTokenAndAdvance(); // (
				auto* grouping = new GroupingExpr();
				grouping->expression = parseExpression();
				getTokenAndAdvance(); // )
				return grouping;
			}

			Token numberToken = getTokenAndAdvance();
			auto* literal = new LiteralExpr();
			literal->value = numberToken.value;
			return literal;
		}
	};
}

Program constructAssembly(const std::vector<Token>& tokens)
{
	Program program;
	program.statements = Parser(tokens).parseStatements();
	return program;
}

Program assemble(const std::string& source)
{
	return constructAssembly(tokenizeSource(source));
}
