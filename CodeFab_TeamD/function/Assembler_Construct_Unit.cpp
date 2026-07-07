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

		// 토큰을 소비하고 커서를 한 칸 옮기기. END_OF_FILE은 넘어가지 않아 범위 밖 접근을 막는다.
		Token getTokenAndAdvance()
		{
			Token token = getCurrentToken();
			if (token.type != TokenType::END_OF_FILE)
				++current;
			return token;
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
			default: return parseExpressionStatement();
			}
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
			getTokenAndAdvance(); // }
			return block;
		}

		// "if ( 조건 ) 문장 (else 문장)?" 을 읽어 IfStmt를 만든다.
		// else는 항상 가장 최근에 열린(재귀 호출 중인) if에 결합되므로 dangling else가 자연히 해결된다.
		Stmt* parseIfStatement()
		{
			getTokenAndAdvance(); // if
			getTokenAndAdvance(); // (
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

		// "이름 = 식" 을 읽어 AssignExpr를 만든다. 우결합이므로 재귀로 처리
		Expr* parseAssignmentExpr()
		{
			Expr* expr = parseComparisonExpr();

			if (getCurrentToken().type == TokenType::EQUAL)
			{
				getTokenAndAdvance(); // =
				Expr* value = parseAssignmentExpr();

				if (auto* variable = dynamic_cast<VariableExpr*>(expr))
				{
					auto* assign = new AssignExpr();
					assign->name = variable->name;
					assign->value = value;
					return assign;
				}
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

			return parsePrimary();
		}

		// 괄호면 GroupingExpr, 식별자면 VariableExpr, 아니면 리터럴(숫자/문자열/불리언)
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

			Token token = getTokenAndAdvance();
			if (token.type == TokenType::IDENTIFIER)
			{
				auto* variable = new VariableExpr();
				variable->name = token;
				return variable;
			}

			auto* literal = new LiteralExpr();
			literal->value = token.value;
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
