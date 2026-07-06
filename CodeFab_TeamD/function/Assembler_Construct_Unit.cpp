#include "../function.h"
#include <stdexcept>

namespace
{
	class Parser
	{
	public:
		explicit Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

		Program parseProgram()
		{
			Program program;
			while (!isAtEnd()) program.statements.push_back(statement());
			return program;
		}

	private:
		const std::vector<Token>& tokens;
		size_t current = 0;

		bool isAtEnd() const { return peek().type == TokenType::END_OF_FILE; }
		const Token& peek() const { return tokens[current]; }
		const Token& advance() { return tokens[current++]; }
		bool check(TokenType type) const { return !isAtEnd() && peek().type == type; }
		bool match(TokenType type) { if (check(type)) { advance(); return true; } return false; }
		const Token& expect(TokenType type, const std::string& message)
		{
			if (check(type)) return advance();
			throw std::runtime_error(message);
		}

		Stmt* statement()
		{
			if (match(TokenType::VAR)) return varDeclStatement();
			if (match(TokenType::PRINT)) return printStatement();
			return expressionStatement();
		}

		Stmt* varDeclStatement()
		{
			Token name = expect(TokenType::IDENTIFIER, "변수 이름이 필요합니다.");
			Expr* initializer = nullptr;
			if (match(TokenType::EQUAL)) initializer = expression();
			expect(TokenType::SEMICOLON, "';'가 필요합니다.");

			auto* stmt = new VarDeclStmt();
			stmt->name = name;
			stmt->initializer = initializer;
			return stmt;
		}

		Stmt* printStatement()
		{
			Expr* value = expression();
			expect(TokenType::SEMICOLON, "';'가 필요합니다.");
			auto* stmt = new PrintStmt();
			stmt->expression = value;
			return stmt;
		}

		Stmt* expressionStatement()
		{
			Expr* value = expression();
			expect(TokenType::SEMICOLON, "';'가 필요합니다.");
			auto* stmt = new ExpressionStmt();
			stmt->expression = value;
			return stmt;
		}

		Expr* expression() { return term(); }

		Expr* term()
		{
			Expr* left = factor();
			while (check(TokenType::PLUS) || check(TokenType::MINUS))
			{
				Token op = advance();
				Expr* right = factor();
				auto* expr = new BinaryExpr();
				expr->left = left;
				expr->op = op;
				expr->right = right;
				left = expr;
			}
			return left;
		}

		Expr* factor()
		{
			Expr* left = primary();
			while (check(TokenType::STAR) || check(TokenType::SLASH))
			{
				Token op = advance();
				Expr* right = primary();
				auto* expr = new BinaryExpr();
				expr->left = left;
				expr->op = op;
				expr->right = right;
				left = expr;
			}
			return left;
		}

		Expr* primary()
		{
			if (match(TokenType::NUMBER) || match(TokenType::STRING))
			{
				auto* expr = new LiteralExpr();
				expr->value = tokens[current - 1].value;
				return expr;
			}
			if (match(TokenType::TRUE))
			{
				auto* expr = new LiteralExpr();
				expr->value = true;
				return expr;
			}
			if (match(TokenType::FALSE))
			{
				auto* expr = new LiteralExpr();
				expr->value = false;
				return expr;
			}
			if (check(TokenType::IDENTIFIER))
			{
				auto* expr = new VariableExpr();
				expr->name = advance();
				return expr;
			}
			if (match(TokenType::LEFT_PAREN))
			{
				Expr* inner = expression();
				expect(TokenType::RIGHT_PAREN, "')'가 필요합니다.");
				auto* expr = new GroupingExpr();
				expr->expression = inner;
				return expr;
			}
			throw std::runtime_error("표현식이 필요합니다.");
		}
	};
}

Program constructAssembly(const std::vector<Token>& tokens)
{
	return Parser(tokens).parseProgram();
}

Program assemble(const std::string& source)
{
	return constructAssembly(tokenizeSource(source));
}
