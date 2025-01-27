#include "parser.h"

#include <ast/exprs/addexpr.h>
#include <ast/exprs/mulexpr.h>
#include <ast/exprs/numberexpr.h>
#include <ast/exprs/specialexpr.h>
#include <ast/exprs/variableexpr.h>
#include <ast/instructions/assigninstruction.h>
#include <ast/instructions/generateinstruction.h>
#include <ast/instructions/instruction.h>

#include <iostream>

ErrorOr<void> Parser::parse_all() {
    expect_newline(false);
    m_instructions = {};
    for (;;) {
        auto maybe_instruction = parse_instruction();
        if (maybe_instruction.is_error()) return false;
        auto instruction = maybe_instruction.value();
        if (!instruction) break;
        expect_newline();
        m_instructions.push_back(instruction);
    }
    auto backtrack = position();
    auto maybe_lex = lex();
    if (maybe_lex.is_error()) return false;
    // If managed to lex, error
    if (maybe_lex.value()) {
        set_position(backtrack);
        set_error("unexpected token");
        return false;
    }
    return true;
}

ErrorOr<bool> Parser::match_token(Token::Type type) {
    auto backtrack = position();
    auto maybe_lex = lex();
    if (maybe_lex.is_error()) return {};
    // If got eof
    if (!maybe_lex.value())
        return false;
    if (type != token().type()) {
        set_position(backtrack);
        return false;
    }
    return true;
}

void Parser::set_error(std::string error_message) {
    m_has_error = true;
    m_error_message = error_message;
}

void Parser::show_error() {
    if (m_lexer.has_error()) {
        m_lexer.show_error();
    } else if (has_error()) {
        auto pos = position();
        std::cerr << "ParserError at " << m_lexer.filename() << "(l" << pos.line
                  << ", c" << pos.column << "): " << m_error_message
                  << std::endl;
    } else {
        assert(0);
    }
}

ErrorOr<void> Parser::expect_newline(bool do_error) {
    if (is_eof()) return true;
    auto maybe_match = match_token(Token::Type::Newline);
    if (maybe_match.is_error()) return false;
    // If matches
    if (maybe_match.value()) return true;

    if (!do_error) return true;

    set_error("expected a newline");
    return false;
}

ErrorOr<AstInstruction::Ptr> Parser::parse_assignment() {
    auto maybe_matched = match_token(Token::Type::Identifier);
    if (maybe_matched.is_error()) return {};
    if (!maybe_matched.value()) return AstInstruction::Ptr(nullptr);

    auto varname = token().to_string();

    maybe_matched = match_token(Token::Type::Is);
    if (maybe_matched.is_error()) return {};
    // If not found "Is"
    if (!maybe_matched.value()) {
        set_error("expected 'is'");
        return {};
    }

    auto maybe_expr = parse_expr();
    if (maybe_expr.is_error()) return {};
    // If not found expr
    auto expr = maybe_expr.value();
    if (!expr) {
        set_error("failed to find expr");
        return {};
    }

    return (AstInstruction::Ptr)std::make_shared<AstAssignInstruction>(varname,
                                                                       expr);
}

ErrorOr<AstInstruction::Ptr> Parser::parse_generate() {
    auto maybe_matched = match_token(Token::Type::Generate);
    if (maybe_matched.is_error()) return {};
    if (!maybe_matched.value()) return AstInstruction::Ptr(nullptr);

    auto maybe_expr = parse_expr();
    if (maybe_expr.is_error()) return {};
    // If not found expr
    auto expr = maybe_expr.value();
    if (!expr) {
        set_error("failed to find expr");
        return {};
    }

    return (AstInstruction::Ptr)std::make_shared<AstGenerateInstruction>(expr);
}

ErrorOr<AstInstruction::Ptr> Parser::parse_instruction() {
    auto maybe_parsed = parse_assignment();
    if (maybe_parsed.is_error()) return {};
    if (maybe_parsed.value()) return maybe_parsed.value();

    maybe_parsed = parse_generate();
    if (maybe_parsed.is_error()) return {};
    if (maybe_parsed.value()) return maybe_parsed.value();

    return AstInstruction::Ptr(nullptr);
}

ErrorOr<AstExpr::Ptr> Parser::parse_number() {
    auto maybe_matched = match_token(Token::Type::Number);
    if (maybe_matched.is_error()) return {};
    // If failed to match
    if (!maybe_matched.value()) return AstExpr::Ptr(nullptr);
    return (AstExpr::Ptr)std::make_shared<AstNumberExpr>(token().to_number());
}

ErrorOr<AstExpr::Ptr> Parser::parse_paren() {
    auto maybe_matched = match_token(Token::Type::LeftParen);
    if (maybe_matched.is_error()) return {};
    // If failed to match
    if (!maybe_matched.value()) return AstExpr::Ptr(nullptr);

    auto maybe_expr = parse_expr();
    if (maybe_expr.is_error()) return {};
    auto expr = maybe_expr.value();
    if (!expr) {
        set_error("expected expr after '('");
        return {};
    }

    maybe_matched = match_token(Token::Type::LeftParen);
    if (maybe_matched.is_error()) return {};
    // If failed to match
    if (!maybe_matched.value()) {
        set_error("expected ')'");
        return {};
    }

    return expr;
}

ErrorOr<AstExpr::Ptr> Parser::parse_variable() {
    auto maybe_matched = match_token(Token::Type::Identifier);
    if (maybe_matched.is_error()) return {};
    if (!maybe_matched.value()) return AstExpr::Ptr(nullptr);
    return (AstExpr::Ptr)std::make_shared<AstVariableExpr>(token().to_string());
}

ErrorOr<AstExpr::Ptr> Parser::parse_special() {
    auto backtrack = position();
    auto maybe_lex = lex();
    if (maybe_lex.is_error()) return {};
    if (!maybe_lex.value()) return AstExpr::Ptr(nullptr);

    AstSpecialExpr::Type special_type;
    switch (token().type()) {
        case Token::Type::LeftArrow:
            special_type = AstSpecialExpr::Type::GoLeft;
            break;
        case Token::Type::RightArrow:
            special_type = AstSpecialExpr::Type::GoRight;
            break;
        case Token::Type::Caret:
            special_type = AstSpecialExpr::Type::GoUp;
            break;
        case Token::Type::Comma:
            special_type = AstSpecialExpr::Type::GoDown;
            break;
        default:
            set_position(backtrack);
            return AstExpr::Ptr(nullptr);
    }

    return (AstExpr::Ptr)std::make_shared<AstSpecialExpr>(special_type);
}

ErrorOr<AstExpr::Ptr> Parser::parse_single() {
    auto maybe_parsed = parse_special();
    if (maybe_parsed.is_error()) return {};
    if (maybe_parsed.value()) return maybe_parsed.value();

    maybe_parsed = parse_number();
    if (maybe_parsed.is_error()) return {};
    if (maybe_parsed.value()) return maybe_parsed.value();

    maybe_parsed = parse_paren();
    if (maybe_parsed.is_error()) return {};
    if (maybe_parsed.value()) return maybe_parsed.value();

    maybe_parsed = parse_variable();
    if (maybe_parsed.is_error()) return {};
    if (maybe_parsed.value()) return maybe_parsed.value();

    maybe_parsed = parse_special();
    if (maybe_parsed.is_error()) return {};
    if (maybe_parsed.value()) return maybe_parsed.value();

    return AstExpr::Ptr(nullptr);
}

ErrorOr<AstExpr::Ptr> Parser::parse_product() {
    auto maybe_parsed = parse_single();
    if (maybe_parsed.is_error()) return {};
    auto expr = maybe_parsed.value();

    for (;;) {
        auto backtrack = position();
        auto maybe_lex = lex();
        if (maybe_lex.is_error()) return AstExpr::Ptr(nullptr);
        // If not lexed, break
        if (!maybe_lex.value()) break;
        if (token().type() != Token::Type::Asterisk) {
            set_position(backtrack);
            break;
        }
        auto maybe_rhs = parse_single();
        if (maybe_rhs.is_error()) return {};
        auto rhs = maybe_rhs.value();
        if (!rhs) {
            set_error("unexpected expr after operator");
            return AstExpr::Ptr(nullptr);
        }
        expr = std::make_shared<AstMulExpr>(expr, rhs);
    }

    return expr;
}

ErrorOr<AstExpr::Ptr> Parser::parse_sum() {
    auto maybe_parsed = parse_product();
    if (maybe_parsed.is_error()) return {};
    auto expr = maybe_parsed.value();

    for (;;) {
        auto backtrack = position();
        auto maybe_lex = lex();
        if (maybe_lex.is_error()) return AstExpr::Ptr(nullptr);
        // If not lexed, break
        if (!maybe_lex.value()) break;
        if (token().type() != Token::Type::Plus) {
            set_position(backtrack);
            break;
        }
        auto maybe_rhs = parse_product();
        if (maybe_rhs.is_error()) return {};
        auto rhs = maybe_rhs.value();
        if (!rhs) {
            set_error("unexpected expr after operator");
            return AstExpr::Ptr(nullptr);
        }
        expr = std::make_shared<AstAddExpr>(expr, rhs);
    }

    return expr;
}
